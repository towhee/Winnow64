#include "Views/filterpanel.h"
#include "Datamodel/filters.h"
#include "Main/global.h"

#include <QVBoxLayout>

FilterPanel::FilterPanel(Filters *f, QWidget *parent)
    : QWidget(parent), filters(f)
{
    if (G::isLogger) G::log("FilterPanel::FilterPanel");

    /*  NOT A FOOTER. It is hidden unless there is a reason the panel cannot show
        anything -- no index, or an empty one -- so the panel is the tree in ordinary
        use. See "Nothing to report is nothing shown" in the header. */
    statusLabel = new QLabel;
    statusLabel->setWordWrap(true);
    statusLabel->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(filters, 1);          // re-parents the tree into this panel
    layout->addWidget(statusLabel);

/*
    THE SEARCH ROW IS IN THE TREE, so the query arrives here as a signal from Filters
    rather than from a widget this panel owns. A QTreeWidget editor commits on Return,
    which is the behaviour a library-sized search needs anyway -- see the header.
*/
    connect(filters, &Filters::searchStringChange,
            this, &FilterPanel::searchTextChanged);

    applyScope();
}

void FilterPanel::setScope(Scope s)
{
/*
    Switch scope, carrying the search TEXT.

    That is the hand-off: a search here that found nothing is one click from being asked
    of the library. Losing the text on the switch would make the two scopes feel like two
    panels again, which is what this replaced.

    The checked items do NOT carry -- see the header. The tree is rebuilt for whichever
    vocabulary it is now showing.
*/
    if (G::isLogger) G::log("FilterPanel::setScope");
    if (s == currentScope) return;
    currentScope = s;
    applyScope();
    /*  Tell MW, which owns G::scope and the Catalog rows above the two trees.
        MW::setScope early-returns when the scope already matches, so calling
        back into this panel from there cannot loop. */
    emit scopeChanged(static_cast<int>(s));
}

void FilterPanel::applyScope()
{
    /*  TIMED UNCONDITIONALLY -- once per scope switch is one line, and this is inside
        the click a person reported as a beachball: it re-points the category tree and is
        what the Catalog row in the Source panel calls into. */
    QElapsedTimer asTimer;
    asTimer.start();

/*
    Point the category tree at the right source.

    THE SEARCH TEXT MOVES BOTH WAYS. Leaving Folders means the proxy filter must stop
    applying the text -- the whole datamodel comes back -- and returning to Folders means
    it must start again, or the loaded set would stay narrowed by a query the panel is no
    longer showing as active.
*/
    if (currentScope == CatalogScope) {
        /*  THE TREE KEEPS THE DATAMODEL'S CATEGORIES, which is the whole change.

            This used to call loadCatalogCategories() and hide the datamodel's own, on the
            reasoning that the catalog holds values the loaded folder does not. That was
            true when a catalog scope loaded a 5,000-row slice; it is not true now that it
            loads the whole catalog -- the datamodel's categories ARE the library's, with
            live counts, and BuildFilters already maintains them.

            It is also what makes filtering behave: a checked item narrows the proxy and
            the category head turns yellow, exactly as in Folders, instead of re-running a
            query that reloaded the model and rebuilt the tree from the survivors. */
        filters->showAllCategories();
        /*  Load the set; the categories follow from it. NOT requested explicitly here --
            the load is asynchronous, so a rebuild asked for now would run against the
            model being replaced. MW::folderChangeCompleted builds the filters when the
            fill finishes, which is the same path a folder load takes.

            FORCED, because the model does not necessarily hold what results describes.
            Coming back from Folders scope the model holds a FOLDER while results still
            names the catalog rows loaded last time, so the "did the result change" guard
            would compare equal and load nothing at all -- a Catalog scope showing one
            folder. The guard is for a re-run of the same query, not for a scope entry. */
        runSearch(true);
    }
    else {
        filters->showAllCategories();
        results.clear();
        totalMatches = 0;
        /* The tree is holding the catalog's values; only MW knows whether the model is
           ready to rebuild them from. It re-applies the search text afterwards, so the
           text survives the switch even though the checks do not. */
        emit rebuildFolderCategoriesRequested();
    }
    refresh();

    if (G::isPerfProbe)
        qDebug().noquote() << "[PERF] FilterPanel::applyScope" << asTimer.elapsed()
                           << "ms  scope ="
                           << (currentScope == CatalogScope ? "Catalog" : "Folders");
}

void FilterPanel::focusSearch()
{
/*
    Open the editor on the Search row. The row is an ordinary tree item, so the category
    has to be shown, expanded and scrolled to first -- Filters::editSearchText does all
    of it, and is also what MW uses when the panel is not in play.
*/
    if (G::isLogger) G::log("FilterPanel::focusSearch");
    filters->editSearchText();
}

void FilterPanel::setScanning(bool on)
{
    scanning = on;
    refresh();
}

void FilterPanel::refresh()
{
    if (G::isLogger) G::log("FilterPanel::refresh");

    /*  runSearch is a no-op when the index could not be opened, so the reason has to be
        written here rather than left to a search that will not run. */
    if (currentScope == CatalogScope && Catalog::instance().isAvailable()) runSearch();
    else updateStatus();
}

CatalogQuery FilterPanel::currentQuery() const
{
/*
    THE TEXT ONLY. The checked category items used to go into the query as well
    (filters->fillQuery), which was right when a check WAS the search; now a check
    narrows the proxy over what is loaded, so putting it in the query too would narrow
    the loaded SET by the same values -- filtering twice, and rebuilding the tree from
    the remainder. What the query decides is which images are loaded at all.
*/
/*
    THE TEXT IS NOT IN IT WHEN THE WHOLE CATALOG IS LOADED, which is the default and the
    documented design ("the catalog is browsed whole"). The query decides WHAT IS LOADED,
    and what is loaded is everything; the box then FILTERS those rows through the proxy,
    exactly as it does in Folders scope. Putting the text here as well would make every
    query a reload of the library -- see filterQueriesDoNotReload below.

    IT IS IN IT WHEN A CAP IS SET. With G::maxSearchResults > 0 the loaded set is only
    the newest N rows, so it is NOT the catalog and the proxy cannot answer for the rest:
    the index has to be asked, and the reload is the honest cost of a cap the user chose.
*/
    CatalogQuery q;
    if (resultLimit() > 0) q.text = filters->currentSearchText();
    return q;
}

void FilterPanel::runSearch(bool force)
{
    if (G::isLogger) G::log("FilterPanel::runSearch");
    if (currentScope != CatalogScope) return;
    if (!Catalog::instance().isAvailable()) return;

    const CatalogQuery q = currentQuery();

    /*  AN EMPTY QUERY IS THE WHOLE CATALOG, newest first (searchRows orders by captured
        DESC), and that is the ordinary case rather than a special one: opening the
        catalog is opening all of it, exactly as opening a folder opens all of it.

        AND IT IS NO LONGER A WINDOW ONTO THE SET. The cap that made it one existed
        because a row cost ~20 KB and had to be read from its own file; neither is true
        now, so G::maxSearchResults defaults to no limit and the whole catalog loads. */
    /*  THE ROWS, NOT THE PATHS. searchRows returns everything a datamodel row displays
        from the query that found it, so loading the result opens no files -- measured at
        5.8x faster than asking for paths and looking each one up. The paths are still
        what "did the result actually change" compares, because comparing whole rows would
        also fire on a rating edited elsewhere. */
    const QStringList previous = resultPaths();
    results = Catalog::instance().searchRows(q, resultLimit(), &totalMatches);
    updateStatus();

    /*  LOADED WITHOUT BEING ASKED, which is the only way it is loaded now: picking a
        folder shows pictures, so picking Catalog -- or narrowing it -- must too. The
        Load and Add buttons that used to sit under this are gone with the search-result
        paradigm they belonged to; a collection, if one is ever built, is a menu command
        over the loaded set, not a button in the filter panel.

        Guarded three ways, because a load is a full model reset plus a metadata read
        and must not run on every keystroke:
          - only when the result set actually CHANGED (typing that narrows nothing, or
            a re-run of the same query, loads nothing);
          - only up to autoLoadMax(), above which the user is asked to narrow first --
            replacing what they have with thousands of images they did not ask for is
            not a good guess;
          - never from an ordinary query at all, unless a cap is set. This runs when the
            scope is switched to Catalog (forced), when the dock is shown, and on a
            Search row edit ONLY in a capped scope. Narrowing an uncapped catalog is a
            proxy filter and reaches none of this -- see searchTextChanged.
    */
    if ((force || resultPaths() != previous) && !results.isEmpty()
        && results.size() <= autoLoadMax())
        emit loadResults(results, false, currentQuery());
}

void FilterPanel::searchTextChanged(const QString &text)
{
/*
    The Search row was edited.

    THE EDIT HAS ALREADY DONE THE WORK. Filters emits searchStringChange (which writes
    G::SearchColumn over the model) and then filterChange (which re-runs the proxy), so
    in both scopes the query has narrowed the view before this is called. There is
    nothing here to run and nothing to load.

    THAT IS THE FIX FOR THE TEN-MINUTE BEACHBALL. Catalog scope used to answer a query by
    asking the INDEX and LOADING the answer. On a 43,070-image catalog "Gloria OR Rory"
    matches 40,667 -- not because of anything odd, but because the user's own name is in
    the creator and copyright of nearly everything they shot -- so running it threw away a
    model holding 43,070 rows and refilled it with 40,667 of the SAME rows: MW::stop,
    clearDataModel, addCatalogRows, BuildFilters over every row, the availability pass,
    and an icon read for the lot. Measured against the real index, the three SQL
    statements cost 0.4 s TOGETHER; all the rest of the ten minutes was the reload.

    It never needed to load anything. Catalog scope already has the WHOLE catalog in the
    model, so every row the query could match is loaded already and the question is a
    FILTER -- which is what the panel has always claimed to be ("in both scopes the panel
    filters; only the set differs") and what the category checkboxes below it have always
    done. A filter change at this scale is ~261 ms.

    THE ONE CASE THAT STILL ASKS THE INDEX is a capped scope -- see currentQuery.
*/
    Q_UNUSED(text)
    if (G::isLogger) G::log("FilterPanel::searchTextChanged", text);

    if (resultLimit() > 0 && currentScope == CatalogScope) runSearch();
    updateStatus();
}

QStringList FilterPanel::resultPaths() const
{
    QStringList out;
    out.reserve(results.size());
    for (const CatalogRow &r : results) out << r.path;
    return out;
}

void FilterPanel::updateStatus()
{
/*
    NOTHING TO REPORT IS NOTHING SHOWN.

    This used to be a footer: a line of prose about what the panel was doing ("Filtering
    the loaded images", "43,064 images found") over a Load and an Add button. All of it
    is gone. Loading a search RESULT was a paradigm the catalog no longer has -- the
    catalog is browsed whole and the panel filters it, so there is no found set to load
    and nothing to add it to; and a running commentary on a filter the user can see the
    effect of in the grid is words for their own sake.

    What is left is the case the panel genuinely cannot answer -- there is no index, or
    there is nothing in it. Those are why-is-this-not-doing-anything facts a user should
    not have to open a dialog to learn, so they are said inline and the label hides again
    the moment they stop being true.
*/
    QString msg;
    if (currentScope == CatalogScope && !Catalog::instance().isAvailable()) {
        msg = "The catalog is unavailable -- the local index database could not be "
              "opened. Browsing and the Folders scope are unaffected.";
    }
    else if (currentScope == CatalogScope && totalMatches == 0 && !scanning) {
        msg = "Nothing catalogued yet. Folders are catalogued as you open them, and "
              "File > Manage Catalog... chooses what is indexed in the background.";
    }

    statusLabel->setText(msg);
    statusLabel->setVisible(!msg.isEmpty());
}
