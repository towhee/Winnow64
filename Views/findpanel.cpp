#include "Views/findpanel.h"
#include "Datamodel/filters.h"
#include "Main/global.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>

FindPanel::FindPanel(Filters *f, QWidget *parent)
    : QWidget(parent), filters(f)
{
    if (G::isLogger) G::log("FindPanel::FindPanel");

    /* The scope control. Two checkable buttons in an exclusive group rather than a combo:
       it is a two-way switch the user flips constantly, and a combo would hide the choice
       that is not current behind a click. */
    foldersBtn = new QToolButton;
    foldersBtn->setText("Folders");
    foldersBtn->setCheckable(true);
    foldersBtn->setChecked(true);
    foldersBtn->setToolTip("Narrow the images loaded from the open folder.\n"
                           "F2 searches the open folder.");

    catalogBtn = new QToolButton;
    catalogBtn->setText("Catalog");
    catalogBtn->setCheckable(true);
    catalogBtn->setToolTip("Search every image Winnow has catalogued, including "
                           "folders that are not open.\n"
                           "Shift+F2 searches the catalog.");

    QButtonGroup *scopeGroup = new QButtonGroup(this);
    scopeGroup->setExclusive(true);
    scopeGroup->addButton(foldersBtn);
    scopeGroup->addButton(catalogBtn);

    updateStyle();

    searchEdit = new QLineEdit;
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setToolTip(
        "Words are AND-ed. Use OR between alternatives, \"quotes\" for a phrase,\n"
        "and -word or NOT word to leave something out.");

    QHBoxLayout *scopeRow = new QHBoxLayout;
    scopeRow->setContentsMargins(0, 0, 0, 0);
    scopeRow->setSpacing(0);
    scopeRow->addWidget(foldersBtn);
    scopeRow->addWidget(catalogBtn);
    scopeRow->addStretch(1);

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

    catalogStatusLabel = new QLabel;
    catalogStatusLabel->setWordWrap(true);

    manageRootsBtn = new QPushButton("Manage...");
    manageRootsBtn->setToolTip("Choose which folders are indexed in the background, and "
                               "scan them now.");
    manageRootsBtn->setStyleSheet("QPushButton { min-width: 0; }");

    QHBoxLayout *statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->addWidget(catalogStatusLabel, 1);
    statusRow->addWidget(manageRootsBtn);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addLayout(scopeRow);
    layout->addWidget(searchEdit);
    layout->addWidget(filters, 1);          // re-parents the tree into this panel
    layout->addWidget(resultLabel);
    layout->addWidget(loadRow);
    layout->addLayout(statusRow);

    debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(kDebounceMs);
    connect(debounce, &QTimer::timeout, this, &FindPanel::runSearch);

    connect(foldersBtn, &QToolButton::clicked, this, [this]{ setScope(FolderScope); });
    connect(catalogBtn, &QToolButton::clicked, this, [this]{ setScope(CatalogScope); });

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

    /* A category item was checked or excluded. In Folders scope Filters has already
       re-run the proxy filter itself; in Catalog it means the query changed. */
    connect(filters, &Filters::filterChange, this, [this](QString){
        if (currentScope == CatalogScope) debounce->start();
        else updateFooter();
    });

    connect(loadBtn, &QPushButton::clicked, this, [this]{
        if (!results.isEmpty()) emit loadResults(results, false);
    });
    connect(addBtn, &QPushButton::clicked, this, [this]{
        if (!results.isEmpty()) emit loadResults(results, true);
    });
    connect(manageRootsBtn, &QPushButton::clicked, this,
            &FindPanel::manageRootsRequested);

    applyScope();
}

void FindPanel::updateStyle()
{
/*
    The scope buttons' colours.

    The app-wide QToolButton rule (Main/widgetcss.cpp) is "background:transparent;
    border:none" with NO :checked state, because every other tool button in Winnow is a
    momentary icon button rather than a toggle. Inheriting it left the two scope buttons
    looking identical whether checked or not -- the control showed no state at all, which
    for a two-way switch is the whole of its job.

    Styled here rather than in widgetcss because this is the only segmented toggle in the
    app: a global :checked rule would light up every other tool button that happens to be
    checkable. Re-applied from MW::setBackgroundShade so it follows the theme rather than
    freezing at whatever shade was in force when the panel was built.
*/
    const QString on   = QColor(G::backgroundShade + 22, G::backgroundShade + 22,
                                G::backgroundShade + 22).name();
    const QString off  = QColor(G::backgroundShade - 6, G::backgroundShade - 6,
                                G::backgroundShade - 6).name();
    const QString edge = QColor(G::backgroundShade + 40, G::backgroundShade + 40,
                                G::backgroundShade + 40).name();
    const QString css = QString(
        "QToolButton {"
        "  background:%1; border:1px solid %3; padding:3px 12px; color:%4;"
        "}"
        "QToolButton:checked {"
        "  background:%2; color:%5; font-weight:bold;"
        "}"
        "QToolButton:hover { background:%2; }"
        "QToolButton:disabled { color:%4; }"
    ).arg(off, on, edge, G::disabledColor.name(), G::textColor.name());
    foldersBtn->setStyleSheet(css);
    catalogBtn->setStyleSheet(css);
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
    foldersBtn->setChecked(s == FolderScope);
    catalogBtn->setChecked(s == CatalogScope);
    applyScope();
}

void FindPanel::applyScope()
{
/*
    Point the category tree at the right source and set the footer's verb.

    THE SEARCH TEXT MOVES BOTH WAYS. Leaving Folders means the proxy filter must stop
    applying the text -- the whole datamodel comes back -- and returning to Folders means
    it must start again, or the loaded set would stay narrowed by a query the panel is no
    longer showing as active.
*/
    if (currentScope == CatalogScope) {
        /* Release the proxy filter. A Catalog query must not ALSO be narrowing what
           is loaded, or the user is looking at two answers at once -- and the tree is
           about to hold catalog values, which mean nothing to the datamodel.
           setSearchText clears the text half; MW::filterChange declines to run the tree
           half while this scope is current. */
        filters->setSearchText(QString());
        if (!filters->loadCatalogCategories()) {
            resultLabel->setText(
                "The catalog is unavailable -- the local index database could not be "
                "opened. Browsing and the Folders scope are unaffected.");
        }
        loadRow->setVisible(true);
        searchEdit->setPlaceholderText("Search every catalogued image...");
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
}

void FindPanel::focusSearch()
{
    searchEdit->setFocus();
    searchEdit->selectAll();
}

void FindPanel::setScanning(bool on)
{
    scanning = on;
    manageRootsBtn->setEnabled(Catalog::instance().isAvailable() && !on);
    refresh();
}

void FindPanel::refresh()
{
    if (G::isLogger) G::log("FindPanel::refresh");

    const bool open = Catalog::instance().isAvailable();
    const int n = open ? Catalog::instance().count() : 0;

    catalogBtn->setEnabled(open);
    manageRootsBtn->setEnabled(open && !scanning);

    /* Always says what is indexed, whether or not a search has run: a search that found
       nothing means something quite different when the catalog is empty, and the user
       should not have to open a dialog to find out which case they are in. */
    if (!open) catalogStatusLabel->setText("Catalog unavailable.");
    else if (scanning) catalogStatusLabel->setText("Cataloguing...");
    else if (n == 0) catalogStatusLabel->setText(
        "Nothing catalogued yet. Folders are catalogued as you open them.");
    else catalogStatusLabel->setText(
        QString("%1 %2 catalogued in %3 %4.")
            .arg(n).arg(n == 1 ? "image" : "images")
            .arg(Catalog::instance().folderCount())
            .arg(Catalog::instance().folderCount() == 1 ? "folder" : "folders"));

    if (currentScope == CatalogScope) runSearch();
    else updateFooter();
}

CatalogQuery FindPanel::currentQuery() const
{
    CatalogQuery q;
    q.text = searchEdit->text();
    filters->fillQuery(q);
    return q;
}

void FindPanel::runSearch()
{
    if (G::isLogger) G::log("FindPanel::runSearch");
    debounce->stop();
    if (currentScope != CatalogScope) return;
    if (!Catalog::instance().isAvailable()) return;

    const CatalogQuery q = currentQuery();

    /* An empty query would match the whole catalog. Reporting "247,000 images" and
       offering to load them is not a useful answer to having asked for nothing, so the
       panel stays quiet until the user has actually said something. */
    if (q.text.trimmed().isEmpty() && !filters->isAnyCatalogFilter()) {
        results.clear();
        totalMatches = 0;
        updateFooter();
        return;
    }

    results = Catalog::instance().search(q, kResultLimit, &totalMatches);
    updateFooter();
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

    if (totalMatches == 0) {
        resultLabel->setText(searchEdit->text().trimmed().isEmpty()
                                     && !filters->isAnyCatalogFilter()
            ? "Type or check something to search the catalog.\n"
              "Counts shown are library totals, not filtered counts."
            : "No matches.");
    }
    else if (results.size() < totalMatches) {
        /* Say plainly that this is a subset, and what would be loaded. */
        resultLabel->setText(QString("%1 matches -- the first %2 will be loaded.")
                                 .arg(totalMatches).arg(results.size()));
    }
    else {
        resultLabel->setText(QString("%1 %2 found.")
                                 .arg(totalMatches)
                                 .arg(totalMatches == 1 ? "image" : "images"));
    }
    loadBtn->setEnabled(!results.isEmpty());
    addBtn->setEnabled(!results.isEmpty());
}
