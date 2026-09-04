#include "Views/findpanel.h"
#include "Datamodel/filters.h"
#include "Main/global.h"

#include <QHBoxLayout>
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

    resultLabel = new QLabel;
    resultLabel->setWordWrap(true);

    loadBtn = new QPushButton("Load");
    loadBtn->setEnabled(false);
    loadBtn->setToolTip("Load the matching images into the grid, replacing what is "
                        "loaded now.");
    addBtn = new QPushButton("Add");
    addBtn->setEnabled(false);
    addBtn->setToolTip("Add the matching images to what is already loaded, instead of "
                       "replacing it.");
    /* min-width: 0 defeats the global "QPushButton { min-width: 100px }" (widgetcss),
       which otherwise becomes a hard floor on how narrow this dock can be made. */
    for (QPushButton *b : {loadBtn, addBtn})
        b->setStyleSheet("QPushButton { min-width: 0; }");

    QHBoxLayout *loadLayout = new QHBoxLayout;
    loadLayout->setContentsMargins(0, 0, 0, 0);
    loadLayout->addWidget(loadBtn, 1);
    loadLayout->addWidget(addBtn, 1);
    loadRow = new QWidget;
    loadRow->setLayout(loadLayout);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(searchEdit);
    layout->addWidget(filters, 1);          // re-parents the tree into this panel
    layout->addWidget(resultLabel);
    layout->addWidget(loadRow);

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

    /*  A category item was checked or excluded. Filters has ALREADY re-run the proxy
        filter, in either scope -- so there is nothing to do here but re-word the footer.

        Catalog scope used to start the debounce and re-run the search instead, which
        reloaded the model with only the matching rows and left BuildFilters to rebuild
        the tree from those: filter on one day and every other day disappeared from the
        tree. A filter narrows what you are looking at; it does not redefine the set. */
    connect(filters, &Filters::filterChange, this, [this](QString){
        updateFooter();
    });

    connect(loadBtn, &QPushButton::clicked, this, [this]{
        if (!results.isEmpty()) emit loadResults(results, false, currentQuery());
    });
    connect(addBtn, &QPushButton::clicked, this, [this]{
        if (!results.isEmpty()) emit loadResults(results, true, currentQuery());
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
    Point the category tree at the right source and set the footer's verb.

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
        loadRow->setVisible(true);
        searchEdit->setPlaceholderText("Search every catalogued image...");
        /*  Load the set; the categories follow from it. NOT requested explicitly here --
            the load is asynchronous, so a rebuild asked for now would run against the
            model being replaced. MW::folderChangeCompleted builds the filters when the
            fill finishes, which is the same path a folder load takes. */
        runSearch();
    }
    else {
        filters->showAllCategories();
        loadRow->setVisible(false);
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

    /*  runSearch is a no-op when the index could not be opened, so the footer -- which
        is now the only place the catalog's state is reported -- has to be written here
        rather than left to a search that will not run. */
    if (currentScope == CatalogScope && Catalog::instance().isAvailable()) runSearch();
    else updateFooter();
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

    /*  AN EMPTY QUERY NOW SHOWS THE MOST RECENT IMAGES rather than nothing.

        It used to report nothing and disable Load, on the reasoning that offering to
        load a quarter of a million images is not a useful answer to having typed
        nothing. But it made Catalog an empty room: the user picked a scope and was shown
        a blank panel, while picking a folder shows pictures immediately. That difference
        IS the paradigm split this removes -- so an empty query is answered the way a
        folder is, with the newest images first (searchRows orders by captured DESC).

        AND IT IS NO LONGER A WINDOW ONTO THE SET. The cap that made it one existed
        because a row cost ~20 KB and had to be read from its own file; neither is true
        now, so G::maxSearchResults defaults to no limit and the whole catalog loads. */
    /*  Nothing ASKED, which is now purely about the text: a checked category is a filter
        over the loaded set, not part of the question that defines it. */
    noQuery = q.text.trimmed().isEmpty();

    /*  THE ROWS, NOT THE PATHS. searchRows returns everything a datamodel row displays
        from the query that found it, so loading the result opens no files -- measured at
        5.8x faster than asking for paths and looking each one up. The paths are still
        what "did the result actually change" compares, because comparing whole rows would
        also fire on a rating edited elsewhere. */
    const QStringList previous = resultPaths();
    results = Catalog::instance().searchRows(q, resultLimit(), &totalMatches);
    updateFooter();

    /*  LOAD WITHOUT BEING ASKED. This is what removes the paradigm split: picking a
        folder shows pictures, so picking Catalog -- or narrowing it -- must too. The
        Load button stays for an explicit reload, and ADD stays because it is the one
        thing this cannot do for the user: combining two searches is a decision, not a
        default.

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

void FindPanel::updateFooter()
{
/*
    The footer says what the panel is currently doing, and the verb changes with the
    scope: Folders is showing a subset of what is loaded, Catalog has found images that
    are not loaded at all and is offering to load them.
*/
    if (currentScope == FolderScope) {
        loadBtn->setEnabled(false);
        addBtn->setEnabled(false);
        resultLabel->setText(filters->isAnyFilter()
            ? "Filtering the loaded images."
            : "No filter -- all loaded images are shown.");
        return;
    }

    /*  THE FOOTER IS THE STATUS LINE NOW. The "N images catalogued ... Manage..." row
        under the panel is gone (the roots editor is File > Manage Catalog...), so what
        it said about the index -- unavailable, scanning, empty -- is said here. A search
        that found nothing means something quite different when nothing is indexed, and
        the user should not have to open a dialog to find out which case they are in. */
    if (!Catalog::instance().isAvailable()) {
        loadBtn->setEnabled(false);
        addBtn->setEnabled(false);
        resultLabel->setText(
            "The catalog is unavailable -- the local index database could not be "
            "opened. Browsing and the Folders scope are unaffected.");
        return;
    }

    /*  A FILTER IS A FILTER IN EITHER SCOPE, so say the same thing about it. The counts
        below describe what is LOADED; once a category is checked the proxy decides what
        is shown, and repeating the catalogued total would answer a question the user has
        moved on from. */
    if (filters->isAnyFilter()) {
        loadBtn->setEnabled(!results.isEmpty());
        addBtn->setEnabled(!results.isEmpty());
        resultLabel->setText("Filtering the catalogued images.");
        return;
    }

    const QString scanNote = scanning ? "\nCataloguing..." : QString();

    if (totalMatches == 0) {
        resultLabel->setText((noQuery
            ? QString("Nothing catalogued yet. Folders are catalogued as you open them.")
            : QString("No matches.")) + scanNote);
    }
    else if (noQuery) {
        /*  No question asked: this is the catalog itself, newest first. With no cap
            (the default) that IS the whole catalog and there is no subset to explain --
            saying "showing the N most recent" when N is everything would invent a limit
            the user does not have. */
        resultLabel->setText(results.size() >= totalMatches
            ? QString("%1 images catalogued in %2 %3 --\n"
                      "search or check a filter to narrow it.%4")
                  .arg(totalMatches)
                  .arg(Catalog::instance().folderCount())
                  .arg(Catalog::instance().folderCount() == 1 ? "folder" : "folders")
                  .arg(scanNote)
            : QString("%1 images catalogued. Showing the %2 most recent --\n"
                      "search or check a filter to narrow it.%3")
                  .arg(totalMatches).arg(results.size()).arg(scanNote));
    }
    else if (results.size() < totalMatches) {
        /* Say plainly that this is a subset, and what would be loaded. */
        resultLabel->setText(QString("%1 matches -- the first %2 will be loaded.%3")
                                 .arg(totalMatches).arg(results.size()).arg(scanNote));
    }
    else {
        resultLabel->setText(QString("%1 %2 found.%3")
                                 .arg(totalMatches)
                                 .arg(totalMatches == 1 ? "image" : "images")
                                 .arg(scanNote));
    }
    loadBtn->setEnabled(!results.isEmpty());
    addBtn->setEnabled(!results.isEmpty());
}
