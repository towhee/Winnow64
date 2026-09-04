#include "Views/findpanel.h"
#include "Datamodel/filters.h"
#include "Main/global.h"

#include <QVBoxLayout>

FindPanel::FindPanel(Filters *f, QWidget *parent)
    : QWidget(parent), filters(f)
{
    if (G::isLogger) G::log("FindPanel::FindPanel");

    searchEdit = new QLineEdit;
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setToolTip(
        "Words are AND-ed. Use OR between alternatives, \"quotes\" for a phrase,\n"
        "and -word or NOT word to leave something out.");

    /*  NOT A FOOTER. It is hidden unless there is a reason the panel cannot show
        anything -- no index, or an empty one -- so the panel is the tree and the box in
        ordinary use. See "Nothing to report is nothing shown" in the header. */
    statusLabel = new QLabel;
    statusLabel->setWordWrap(true);
    statusLabel->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(searchEdit);
    layout->addWidget(filters, 1);          // re-parents the tree into this panel
    layout->addWidget(statusLabel);

    debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(kDebounceMs);
    connect(debounce, &QTimer::timeout, this, &FindPanel::runSearch);

    connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString &t){
        if (currentScope == FolderScope) {
            /* Folders is not debounced: the proxy filter is already what the panel is
               for, it runs over the loaded rows only, and a delay on it would feel like
               the panel had stopped responding. */
            filters->setSearchText(t);
        }
        else debounce->start();
    });
    connect(searchEdit, &QLineEdit::returnPressed, this, [this]{
        if (currentScope == CatalogScope) runSearch();
    });

    applyScope();
}

void FindPanel::setScope(Scope s)
{
/*
    Switch scope, carrying the search TEXT.

    That is the hand-off: a search here that found nothing is one click from being asked
    of the library. Losing the text on the switch would make the two scopes feel like two
    panels again, which is what this replaced.

    The checked items do NOT carry -- see the header. The tree is rebuilt for whichever
    vocabulary it is now showing.
*/
    if (G::isLogger) G::log("FindPanel::setScope");
    if (s == currentScope) return;
    currentScope = s;
    applyScope();
    /*  Tell MW, which owns G::scope and the Catalog rows above the two trees.
        MW::setScope early-returns when the scope already matches, so calling
        back into this panel from there cannot loop. */
    emit scopeChanged(static_cast<int>(s));
}

void FindPanel::applyScope()
{
    /*  TIMED UNCONDITIONALLY -- once per scope switch is one line, and this is inside
        the click a person reported as a beachball: it re-points the category tree and is
        what the Catalog row in the Folders panel calls into. */
    QElapsedTimer asTimer;
    asTimer.start();

/*
    Point the category tree at the right source and re-word the search box.

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
        searchEdit->setPlaceholderText("Filter every catalogued image...");
        /*  Load the set; the categories follow from it. NOT requested explicitly here --
            the load is asynchronous, so a rebuild asked for now would run against the
            model being replaced. MW::folderChangeCompleted builds the filters when the
            fill finishes, which is the same path a folder load takes. */
        runSearch();
    }
    else {
        filters->showAllCategories();
        results.clear();
        totalMatches = 0;
        searchEdit->setPlaceholderText("Filter the loaded images...");
        /* The tree is holding the catalog's values; only MW knows whether the model is
           ready to rebuild them from. It re-applies the search text afterwards, so the
           text survives the switch even though the checks do not. */
        emit rebuildFolderCategoriesRequested();
    }
    refresh();

    if (G::isPerfProbe)
        qDebug().noquote() << "[PERF] FindPanel::applyScope" << asTimer.elapsed()
                           << "ms  scope ="
                           << (currentScope == CatalogScope ? "Catalog" : "Folders");
}

void FindPanel::focusSearch()
{
    searchEdit->setFocus();
    searchEdit->selectAll();
}

void FindPanel::setScanning(bool on)
{
    scanning = on;
    refresh();
}

void FindPanel::refresh()
{
    if (G::isLogger) G::log("FindPanel::refresh");

    /*  runSearch is a no-op when the index could not be opened, so the reason has to be
        written here rather than left to a search that will not run. */
    if (currentScope == CatalogScope && Catalog::instance().isAvailable()) runSearch();
    else updateStatus();
}

CatalogQuery FindPanel::currentQuery() const
{
/*
    THE TEXT ONLY. The checked category items used to go into the query as well
    (filters->fillQuery), which was right when a check WAS the search; now a check
    narrows the proxy over what is loaded, so putting it in the query too would narrow
    the loaded SET by the same values -- filtering twice, and rebuilding the tree from
    the remainder. What the query decides is which images are loaded at all.
*/
    CatalogQuery q;
    q.text = searchEdit->text();
    return q;
}

void FindPanel::runSearch()
{
    if (G::isLogger) G::log("FindPanel::runSearch");
    debounce->stop();
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
          - never while the search box is mid-word, which the debounce already
            enforces: runSearch only reaches here 250 ms after the last keystroke.
    */
    if (resultPaths() != previous && !results.isEmpty()
        && results.size() <= autoLoadMax())
        emit loadResults(results, false, currentQuery());
}

QStringList FindPanel::resultPaths() const
{
    QStringList out;
    out.reserve(results.size());
    for (const CatalogRow &r : results) out << r.path;
    return out;
}

void FindPanel::updateStatus()
{
/*
    NOTHING TO REPORT IS NOTHING SHOWN.

    This used to be a footer: a line of prose about what the panel was doing ("Filtering
    the loaded images", "43,064 images found") over a Load and an Add button. All of it
    is gone. Loading a search RESULT was a paradigm the catalog no longer has -- the
    catalog is browsed whole and the panel filters it, so there is no found set to load
    and nothing to add it to; and a running commentary on a filter the user can see the
    effect of in the grid is words for their own sake.

    What is left is the case the panel genuinely cannot answer: there is no index, or
    there is nothing in it. Those are why-is-this-empty facts a user should not have to
    open a dialog to learn, so they are said inline and the label hides again the moment
    they stop being true.
*/
    if (currentScope != CatalogScope) {
        statusLabel->setVisible(false);
        return;
    }

    QString msg;
    if (!Catalog::instance().isAvailable()) {
        msg = "The catalog is unavailable -- the local index database could not be "
              "opened. Browsing and the Folders scope are unaffected.";
    }
    else if (totalMatches == 0 && !scanning) {
        msg = "Nothing catalogued yet. Folders are catalogued as you open them, and "
              "File > Manage Catalog... chooses what is indexed in the background.";
    }

    statusLabel->setText(msg);
    statusLabel->setVisible(!msg.isEmpty());
}
