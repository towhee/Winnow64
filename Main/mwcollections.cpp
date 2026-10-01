#include "Main/mainwindow.h"
#include "Datamodel/collectionstore.h"
#include "Dialogs/querybuilder.h"
#include "Utilities/fileops.h"

#include <QInputDialog>

/*
    COLLECTIONS AND QUERIES -- the Library's user-made sets, Lightroom's collections and
    smart collections.

    BOTH ARE FILTERS. The whole Library stays loaded; each collection and each query is
    an item in the Filters panel's Collections / Queries category, which narrows the
    Library and combines with every other category (OR within, AND across, Opt+click to
    exclude).
      o A collection filters on G::CollectionsColumn, answered by DataModel from a
        membership side table kept here (refreshCollectionMembership).
      o A query carries its query text (Utilities/queryexpr.h) and is evaluated live
        per row by the compiled predicate (SortFilter::compileFilters), so an edit to a
        rating or a keyword is seen at the next refilter.
    Each panel is a second view of its category, as LibTree is of the Folders category.

    THE QUERY BUILDER (Dialogs/querybuilder.h) edits a saved Query (New / Edit in the
    Queries panel) AND the Filters Search row's ad hoc query ("Build query..."), because
    the Search row speaks the same grammar.

    The store is Datamodel/collectionstore.h (its own file, collections.db, because
    neither can be rebuilt from the images); the panels are Views/collectiontree.h.
    See notes/Documentation.txt "Collections (and the Queries to Come)" and "Queries and
    the Query Builder".
*/

using Kind = CollectionStore::Kind;

CollectionTree *MW::setTree(Kind kind) const
{
    return kind == Kind::Query ? queryTree : collectionTree;
}

QTreeWidgetItem *MW::setCategory(Kind kind) const
{
    if (!filters) return nullptr;
    return kind == Kind::Query ? filters->queries : filters->collections;
}

DockWidget *MW::buildLibrarySetDock(Kind kind, const QString &tabText,
                                    const QString &objectName)
{
/*
    ONE BUILDER FOR BOTH PANELS. Usable only while the Library is the source; in Folders
    the tree is greyed and a line above it says why and how to get there, rather than
    the panel disappearing -- a panel that comes and goes with the source is a panel
    nobody finds (feedback: disable with a reason).
*/
    const bool isQuery = kind == Kind::Query;
    dockTextNames << tabText;
    DockWidget *dock = new DockWidget(tabText, objectName, this);
    dock->setObjectName(objectName);

    QWidget *body = new QWidget(dock);
    QVBoxLayout *layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QLabel *reason = new QLabel(body);
    reason->setWordWrap(true);
    reason->setVisible(false);
    layout->addWidget(reason);

    CollectionTree *tree = new CollectionTree(kind, "(99999", 10, body);
    layout->addWidget(tree, 1);

    /*  THE EMPTY STATE SAYS HOW TO BEGIN: a blank panel with no affordance is the one
        thing a new user cannot learn anything from. */
    QLabel *hint = new QLabel(isQuery
        ? tr("No queries yet.\n\nClick + above, or right-click here, to build one -- or "
             "save the Filters Search row's query with \"Save as Query...\".")
        : tr("No collections yet.\n\nClick + above, or right-click here, to make one. "
             "Then drag thumbnails onto it."), body);
    hint->setWordWrap(true);
    hint->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    hint->setVisible(false);
    layout->addWidget(hint);

    dock->setWidget(body);
    dock->setFloating(false);
    dock->setVisible(false);
    connect(dock, &DockWidget::focus, this, &MW::focusOnDock);

    connect(tree, &CollectionTree::filterRequested, this,
            [this, kind](const QVector<qint64> &ids, bool withSub) {
        applySetFilter(kind, ids, withSub);
    });
    connect(tree, &CollectionTree::cancelRequested, this,
            [this, kind] { cancelSetFilter(kind); });
    const QString expandKey = isQuery ? "QueryTreeExpanded" : "CollectionTreeExpanded";
    connect(tree, &CollectionTree::expansionChanged, this, [this, tree, expandKey] {
        QVariantList ids;
        for (qint64 id : tree->expandedIds()) ids << id;
        settings->setValue(expandKey, ids);
    });

    // title bar: New, Cancel, collapse, close
    QHBoxLayout *titleLayout = new QHBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(0);
    DockTitleBar *titleBar = new DockTitleBar(tabText, titleLayout);
    dock->setTitleBarWidget(titleBar);
    titleBar->setToolTip(dockTabToolTip(tabText));

    /*  A + ON THE TITLE BAR, as well as the context menu: making the first one is the
        step everything else depends on, so it gets a visible button. */
    BarBtn *newBtn = new BarBtn();
    newBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    newBtn->setToolTip(isQuery ? tr("New query...") : tr("New collection"));
    connect(newBtn, &BarBtn::clicked, this, [tree] {
        if (tree->isEnabled()) tree->createNode(0);
    });
    titleLayout->addWidget(newBtn);
    titleLayout->addSpacing(10);

    /*  CANCEL: the whole Library, other filters kept. Disabled (with the reason in its
        tooltip) while nothing in this panel is filtering. */
    BarBtn *cancelBtn = new BarBtn();
    cancelBtn->setIcon(":/images/icon16/reset.png", G::iconOpacity);
    connect(cancelBtn, &BarBtn::clicked, this, [this, kind] { cancelSetFilter(kind); });
    titleLayout->addWidget(cancelBtn);
    titleLayout->addSpacing(10);

    if (G::useDWCollapse) {
        BarBtn *collapseBtn = new BarBtn();
        collapseBtn->setIcon(":/images/icon16/collapse.png", G::iconOpacity);
        collapseBtn->setToolTip("Collapse panel.");
        connect(collapseBtn, &BarBtn::clicked, dock, &DockWidget::toggleCollapsed);
        connect(dock, &DockWidget::collapsedChanged, collapseBtn, [collapseBtn](bool c) {
            collapseBtn->setIcon(c ? ":/images/icon16/expand.png"
                                   : ":/images/icon16/collapse.png", G::iconOpacity);
            collapseBtn->setToolTip(c ? "Expand panel." : "Collapse panel.");
        });
        titleLayout->addWidget(collapseBtn);
        titleLayout->addSpacing(10);
    }

    BarBtn *closeBtn = new BarBtn();
    closeBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    closeBtn->setToolTip(isQuery ? tr("Hide the Queries Panel")
                                 : tr("Hide the Collections Panel"));
    connect(closeBtn, &BarBtn::clicked, this,
            isQuery ? &MW::closeQueriesDock : &MW::closeCollectionsDock);
    titleLayout->addWidget(closeBtn);
    titleLayout->addSpacing(5);

    QList<qint64> expandedIds;
    for (const QVariant &v : settings->value(expandKey).toList()) expandedIds << v.toLongLong();
    tree->reload();
    tree->setExpandedIds(expandedIds);

    if (isQuery) {
        queryTree = tree;
        queriesReason = reason;
        queriesEmptyHint = hint;
        queriesCancelBtn = cancelBtn;
    }
    else {
        collectionTree = tree;
        collectionsReason = reason;
        collectionsEmptyHint = hint;
        collectionsCancelBtn = cancelBtn;
    }
    return dock;
}

void MW::createCollectionsDock()
{
    if (G::isLogger) G::log("MW::createCollectionsDock");
    collectionsDockTabText = "Collections";
    collectionsDock = buildLibrarySetDock(Kind::Collection, collectionsDockTabText,
                                          "CollectionsDock");

    connect(collectionTree, &CollectionTree::addPathsRequested,
            this, &MW::addPathsToCollection);
    connect(collectionTree, &CollectionTree::addSelectionRequested,
            this, &MW::addSelectionToCollection);
    connect(collectionTree, &CollectionTree::removeSelectionRequested,
            this, &MW::removeSelectionFromCollection);

    /*  The store drives the Filters categories' items, the membership side table behind
        G::CollectionsColumn, the name resolver for collection:"Name", and the hints.
        The trees reload themselves from the same signals (CollectionTree's
        constructor). */
    CollectionStore &store = CollectionStore::instance();
    connect(&store, &CollectionStore::nodesChanged, this, [this](Kind k) {
        if (k == Kind::Collection) refreshCollectionNodes();
        else refreshQueryNodes();
        updateCollectionsAvailability();
    });
    connect(&store, &CollectionStore::membersChanged,
            this, &MW::refreshCollectionMembership);
    /*  EVERY FILTER BUILD BLANKS THE COUNTS (Filters::clearAll / uncheckAllFilters walk
        every item), and a build follows every load; so the counts are put back after
        it. The categories' items and checks survive a build -- BuildFilters does not own
        them -- which is what lets the Library's saved filters restore into them. */
    connect(buildFilters, &BuildFilters::finishedBuildFilters,
            this, &MW::updateCollectionCounts);
    connect(buildFilters, &BuildFilters::finishedBuildFilters,
            this, &MW::updateQueryCounts);

    /*  collection:"Trip 2024" in a query names a collection; the column holds ids. */
    Query::setResolver([this](const QString &fieldKey, const QString &value) {
        if (fieldKey != "collection") return QStringList();
        return collectionIdsByName.value(value.toCaseFolded());
    });

    /*  The Search row asks for the builder and for "Save as Query". */
    connect(filters, &Filters::queryBuilderRequested, this, &MW::openQueryBuilderForSearch);
    connect(filters, &Filters::saveAsQueryRequested, this, [this](const QString &text) {
        saveQueryAs(Query::Expr::parse(text));
    });

    refreshCollectionNodes();
    refreshCollectionMembership();
    syncSetCancel(Kind::Collection);
    updateCollectionsAvailability();
}

void MW::createQueriesDock()
{
/*
    THE QUERIES DOCK: saved searches, Lightroom's smart collections. Built after the
    Collections dock, whose store connections it shares.
*/
    if (G::isLogger) G::log("MW::createQueriesDock");
    queriesDockTabText = "Queries";
    queriesDock = buildLibrarySetDock(Kind::Query, queriesDockTabText, "QueriesDock");
    connect(queryTree, &CollectionTree::newQueryRequested, this, &MW::newQuery);
    connect(queryTree, &CollectionTree::editQueryRequested, this, &MW::editQuery);

    migrateSavedSearchQueries();
    refreshQueryNodes();
    syncSetCancel(Kind::Query);
    updateCollectionsAvailability();
}

void MW::showCollectionsDock()
{
    if (G::isLogger) G::log("MW::showCollectionsDock");
    if (G::isInitializing || !collectionsDock) return;
    const bool wanted = collectionsDockVisibleAction->isChecked();
    collectionsDock->setVisible(wanted);
    if (wanted) collectionsDock->raise();
}

void MW::closeCollectionsDock()
{
    if (!collectionsDock) return;
    collectionsDock->setVisible(false);
    collectionsDockVisibleAction->setChecked(false);
}

void MW::showQueriesDock()
{
    if (G::isLogger) G::log("MW::showQueriesDock");
    if (G::isInitializing || !queriesDock) return;
    const bool wanted = queriesDockVisibleAction->isChecked();
    queriesDock->setVisible(wanted);
    if (wanted) queriesDock->raise();
}

void MW::closeQueriesDock()
{
    if (!queriesDock) return;
    queriesDock->setVisible(false);
    queriesDockVisibleAction->setChecked(false);
}

void MW::setQueriesDockVisibility()
{
    if (G::isLogger) G::log("MW::setQueriesDockVisibility");
    if (!queriesDock) return;
    queriesDock->setVisible(queriesDockVisibleAction->isChecked());
}

void MW::updateCollectionsAvailability()
{
/*
    Greyed in Folders, with the reason; greyed with the store's reason if collections.db
    cannot be used. The trees stay on screen either way, so the user can see what they
    would get.
*/
    CollectionStore &store = CollectionStore::instance();
    for (Kind kind : {Kind::Collection, Kind::Query}) {
        CollectionTree *tree = setTree(kind);
        if (!tree) continue;
        const bool isQuery = kind == Kind::Query;
        QString reason;
        if (!store.isAvailable())
            reason = store.unavailableReason();
        else if (G::scope != G::Scope::Catalog)
            reason = isQuery
                ? tr("Queries are part of the Library. Choose Library at the top left of "
                     "the window to filter by them.")
                : tr("Collections are part of the Library. Choose Library at the top left "
                     "of the window to filter by them or add to them.");
        const bool usable = reason.isEmpty();
        QLabel *reasonLabel = isQuery ? queriesReason : collectionsReason;
        QLabel *hint = isQuery ? queriesEmptyHint : collectionsEmptyHint;
        tree->setEnabled(usable);
        const QString css = QString("color: %1;").arg(G::disabledColor.name());
        reasonLabel->setText(reason);
        reasonLabel->setStyleSheet(css);
        reasonLabel->setVisible(!usable);
        hint->setStyleSheet(css);
        hint->setVisible(usable && tree->topLevelItemCount() == 0);
    }
    /*  The Filters categories follow: hidden in Folders, where they are unchecked too --
        a hidden check would go on filtering where nobody could see it. */
    if (filters) filters->setLibrarySetsAvailable(G::scope == G::Scope::Catalog
                                                  && store.isAvailable());
}

void MW::refreshCollectionNodes()
{
    if (!filters) return;
    QVector<Filters::CollectionItem> items;
    collectionIdsByName.clear();
    for (const CollectionStore::Node &n : CollectionStore::instance().nodes(Kind::Collection)) {
        items << Filters::CollectionItem{QString::number(n.id),
                                         n.parent > 0 ? QString::number(n.parent)
                                                      : QString(),
                                         n.name, QString()};
        collectionIdsByName[n.name.toCaseFolded()] << QString::number(n.id);
    }
    filters->setSetNodes(filters->collections, items);
    updateCollectionCounts();
    /*  The Queries category too: a query that names a collection resolves the name when
        it is compiled, so its counts change with the collections. */
    refreshQueryNodes();
    syncCollectionTreeFromFilters();
}

void MW::refreshQueryNodes()
{
/*
    The Queries category's items, each with its query as TEXT (what the compiled
    predicate parses), and the Search row's "Load query" list.
*/
    if (!filters) return;
    QVector<Filters::CollectionItem> items;
    QList<QPair<QString, QString>> saved;
    for (const CollectionStore::Node &n : CollectionStore::instance().nodes(Kind::Query)) {
        const QString text = Query::Expr::fromJsonText(n.definition).toText();
        items << Filters::CollectionItem{QString::number(n.id),
                                         n.parent > 0 ? QString::number(n.parent)
                                                      : QString(),
                                         n.name, text};
        if (!text.isEmpty()) saved << qMakePair(n.name, text);
    }
    std::sort(saved.begin(), saved.end(), [](const auto &a, const auto &b) {
        return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
    });
    filters->setSetNodes(filters->queries, items);
    filters->setSavedQueries(saved);
    updateQueryCounts();
    syncCollectionTreeFromFilters();
}

void MW::refreshCollectionMembership()
{
/*
    Rebuild G::CollectionsColumn's side table: source path -> ids of the collections the
    image is directly in.

    KEYED BY THE PATH A LIBRARY ROW WAS LOADED WITH. The store keys members on the
    normalised pathkey; the catalog maps each key back to image.path, which is the
    exact string every Library row holds -- so DataModel::data answers with one hash
    lookup per row, and no row ever has its key recomputed during a filter pass. Two
    statements in all, whatever the Library's size.

    A MEMBERSHIP CHANGE CAN CHANGE WHAT A CHECKED COLLECTION -- OR A QUERY THAT NAMES
    ONE -- ADMITS, so a refilter follows when either category is filtering.
*/
    if (G::isLogger) G::log("MW::refreshCollectionMembership");
    if (!dm) return;
    const QHash<QString, QStringList> byKey =
        CollectionStore::instance().membershipByKey(Kind::Collection);
    QHash<QString, QStringList> byPath;
    if (!byKey.isEmpty()) {
        const QHash<QString, QString> paths = Catalog::instance().pathsForKeys(byKey.keys());
        for (auto it = byKey.constBegin(); it != byKey.constEnd(); ++it) {
            const QString p = paths.value(it.key());
            if (!p.isEmpty()) byPath.insert(p, it.value());
        }
    }
    dm->setCollectionMembership(byPath);
    updateCollectionCounts();
    updateQueryCounts();

    if (!filters) return;
    for (Kind k : {Kind::Collection, Kind::Query}) {
        QStringList inc, exc;
        filters->setFilterState(setCategory(k), inc, exc);
        if (!inc.isEmpty() || !exc.isEmpty()) {
            filterChange("MW::refreshCollectionMembership");
            return;
        }
    }
}

void MW::updateCollectionCounts()
{
/*
    Loaded images per collection -- one pass over the rows, and none at all when nothing
    is in a collection. Rows, as every other category counts them (a version is a row).
*/
    if (!filters || !dm) return;
    QHash<QString, int> counts;
    if (G::scope == G::Scope::Catalog && dm->hasCollectionMembership()) {
        const int n = dm->rowCount();
        for (int row = 0; row < n; ++row) {
            const QStringList ids =
                dm->index(row, G::CollectionsColumn).data(Qt::EditRole).toStringList();
            for (const QString &id : ids) ++counts[id];
        }
    }
    filters->setSetCounts(filters->collections, counts);
}

void MW::updateQueryCounts()
{
/*
    Loaded images each query matches, for the Filters category and the Queries panel.
    Every query over every row -- queries x rows evaluations, each a handful of column
    reads -- so it runs only after a build (and when the queries change), never per
    filter change. An empty query matches every row.
*/
    if (!filters || !dm) return;
    QHash<QString, int> counts;
    QHash<qint64, int> treeCounts;
    QElapsedTimer t;
    t.start();
    if (G::scope == G::Scope::Catalog && !G::isLoadRunning) {
        QVector<QPair<qint64, Query::Expr>> qs;
        for (const CollectionStore::Node &n :
             CollectionStore::instance().nodes(Kind::Query)) {
            Query::Expr e = Query::Expr::fromJsonText(n.definition);
            e.prepare();
            qs << qMakePair(n.id, e);
        }
        const int rows = dm->rowCount();
        for (const auto &q : std::as_const(qs)) {
            int hits = 0;
            for (int row = 0; row < rows; ++row) {
                if (q.second.matches([this, row](int column) {
                        return dm->index(row, column).data(Qt::EditRole);
                    })) ++hits;
            }
            counts.insert(QString::number(q.first), hits);
            treeCounts.insert(q.first, hits);
        }
        /*  MEASURED, not guessed: queries x rows on the GUI thread is the one cost here
            that grows with the Library. If it shows up, move it to a pool thread over a
            snapshot, as the filter build is. */
        if (G::isPerfProbe)
            qDebug().noquote() << "[PERF] updateQueryCounts" << qs.size() << "queries x"
                               << rows << "rows =" << t.elapsed() << "ms";
    }
    filters->setSetCounts(filters->queries, counts);
    if (queryTree) queryTree->setCounts(treeCounts);
}

void MW::syncCollectionTreeFromFilters()
{
/*
    Push both categories' includes into their panels. Called after every filter change
    and every build (from syncLibTreeFromFilters), so a check made in the Filters panel,
    Clear All, or a restored Library filter all show in the panels too.
*/
    if (!filters) return;
    for (Kind k : {Kind::Collection, Kind::Query}) {
        CollectionTree *tree = setTree(k);
        if (!tree) continue;
        QStringList inc, exc;
        filters->setFilterState(setCategory(k), inc, exc);
        QVector<qint64> ids;
        for (const QString &s : inc) ids << s.toLongLong();
        tree->syncFromFilter(ids);
        syncSetCancel(k);
    }
}

void MW::syncSetCancel(Kind kind)
{
    BarBtn *btn = kind == Kind::Query ? queriesCancelBtn : collectionsCancelBtn;
    CollectionTree *tree = setTree(kind);
    if (!btn) return;
    const bool filtering = tree && !tree->checkedIds().isEmpty();
    const QString what = kind == Kind::Query ? tr("query") : tr("collection");
    btn->setEnabled(filtering);
    btn->setToolTip(filtering
        ? tr("Cancel: stop filtering by %1 and show the whole Library, keeping the other "
             "filters.").arg(what)
        : tr("Cancel: nothing to cancel -- no %1 is filtering.").arg(what));
}

void MW::applyCollectionFilter(const QVector<qint64> &ids, bool withSub)
{
    applySetFilter(Kind::Collection, ids, withSub);
}

void MW::applyQueryFilter(const QVector<qint64> &ids, bool withSub)
{
    applySetFilter(Kind::Query, ids, withSub);
}

void MW::applySetFilter(Kind kind, const QVector<qint64> &ids, bool withSub)
{
/*
    A panel click: clear EVERY filter -- all categories, the search text, the pick /
    rating / colour actions, as Clear All does -- then set this category to these nodes
    (and, with withSub, every node inside them), in ONE filterChange. A click means "show
    me this collection / query", not "narrow what I am looking at". An empty set (Cmd+click
    off the last node) is Cancel, which clears only this category.

    NOT WHILE A LOAD IS RUNNING. The load's build resets the categories' checks when it
    lands, so a check made now would be swept away unseen; say so instead.
*/
    if (G::isLogger) G::log("MW::applySetFilter", QString::number(ids.size()));
    if (!filters || !dm || G::scope != G::Scope::Catalog) return;
    if (G::isLoadRunning || G::isModifyingDatamodel || filters->buildingFilters) {
        if (G::popup)
            G::popup->showPopup(tr("The Library is still loading. Click again when it has "
                                   "finished."), 2500);
        syncCollectionTreeFromFilters();
        return;
    }

    QStringList inc;
    CollectionStore &store = CollectionStore::instance();
    for (qint64 id : ids) {
        const QVector<qint64> set = withSub ? store.subtree(id) : QVector<qint64>{id};
        for (qint64 s : set) {
            const QString t = QString::number(s);
            if (!inc.contains(t)) inc << t;
        }
    }
    if (inc.isEmpty()) {
        cancelSetFilter(kind);
        return;
    }

    if (!G::allMetadataAttempted) loadEntireMetadataCache("FilterChange");
    uncheckAllFilters();
    filters->searchString = "";
    dm->searchStringChange("");
    /*  setSetFilter emits the filterChange when it checks a node. If none of these nodes
        has a Filters item it checks nothing and emits nothing, so the clear above must
        still be applied. */
    filters->setSetFilter(setCategory(kind), inc, {});
    QStringList nowInc, nowExc;
    filters->setFilterState(setCategory(kind), nowInc, nowExc);
    if (nowInc.isEmpty()) filterChange("MW::applySetFilter");
    syncCollectionTreeFromFilters();
}

void MW::cancelSetFilter(Kind kind)
{
/*
    Cancel: the whole Library, with every other filter as it is. Only this category is
    cleared -- includes and excludes -- in one filterChange.
*/
    if (G::isLogger) G::log("MW::cancelSetFilter");
    if (!filters) return;
    filters->setSetFilter(setCategory(kind), {}, {});
    syncCollectionTreeFromFilters();
}

/* ---------------------------------------------------------------------------------
   The Query Builder
   --------------------------------------------------------------------------------- */

QStringList MW::querySuggestions(const QString &fieldKey)
{
/*
    THE VALUES THE LIBRARY HOLDS for a field, for the builder's typeahead: collections
    and pick/label choices by name, everything else read off the loaded rows (distinct,
    capped so a per-image field such as a file name stays cheap). Numbers and dates are
    not offered -- a list of every ISO is no help in typing one.
*/
    if (fieldKey == "collection") {
        QStringList names;
        for (const CollectionStore::Node &n :
             CollectionStore::instance().nodes(Kind::Collection)) names << n.name;
        return names;
    }
    const Query::Field *f = Query::field(fieldKey);
    if (!f || !dm) return {};
    if (f->type == Query::Type::Number || f->type == Query::Type::Date
        || f->type == Query::Type::Words || f->type == Query::Type::Enum) return {};
    constexpr int kMaxValues = 5000;
    QSet<QString> seen;
    const int rows = dm->rowCount();
    for (int row = 0; row < rows && seen.size() < kMaxValues; ++row) {
        const QVariant v = dm->index(row, f->column).data(Qt::EditRole);
        if (v.typeId() == QMetaType::QStringList) {
            for (const QString &s : v.toStringList())
                if (!s.trimmed().isEmpty()) seen.insert(s.trimmed());
        }
        else {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty()) seen.insert(s);
        }
    }
    QStringList out(seen.begin(), seen.end());
    out.sort(Qt::CaseInsensitive);
    return out;
}

QPair<int, int> MW::countQueryMatches(const Query::Expr &query)
{
/*
    What the builder's Test reports: how many loaded rows the query matches, out of how
    many -- rows, as every Filters count is (a version is a row). The other filters are
    not applied: the question is what THIS query admits.
*/
    if (!dm) return {0, 0};
    Query::Expr e = query;
    e.prepare();
    const int rows = dm->rowCount();
    int hits = 0;
    for (int row = 0; row < rows; ++row)
        if (e.matches([this, row](int column) {
                return dm->index(row, column).data(Qt::EditRole);
            })) ++hits;
    return {hits, rows};
}

void MW::openQueryBuilderForSearch()
{
/*
    The Search row's "Build query...": the builder on the row's text; Apply writes the
    canonical text back through Filters::setSearchText, the one way the row is set.
    "Save as Query..." inside it keeps the query in the Queries panel as well.
*/
    if (G::isLogger) G::log("MW::openQueryBuilderForSearch");
    if (!filters) return;
    QueryBuilderDialog dlg(QueryBuilderDialog::AdHoc, this);
    dlg.builder()->setSuggestions([this](const QString &k) { return querySuggestions(k); });
    dlg.setCounter([this](const Query::Expr &e) { return countQueryMatches(e); });
    dlg.setExpr(Query::Expr::parse(filters->currentSearchText()));
    connect(&dlg, &QueryBuilderDialog::saveAsQueryRequested,
            this, [this](const Query::Expr &e) { saveQueryAs(e); });
    if (dlg.exec() != QDialog::Accepted) return;
    filters->setSearchText(dlg.expr().toText());
}

void MW::saveQueryAs(const Query::Expr &e)
{
    if (G::isLogger) G::log("MW::saveQueryAs");
    if (e.isEmpty()) {
        if (G::popup) G::popup->showPopup(tr("There is no query to save."), 2000);
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save as Query"), tr("Name:"),
                                               QLineEdit::Normal, e.toText().left(60), &ok)
                             .trimmed();
    if (!ok || name.isEmpty()) return;
    const qint64 id = CollectionStore::instance().create(Kind::Query, 0, name,
                                                         e.toJsonText());
    if (id && G::popup)
        G::popup->showPopup(tr("Saved \"%1\" in the Queries panel.").arg(name), 2000);
}

void MW::newQuery(qint64 parent)
{
    if (G::isLogger) G::log("MW::newQuery");
    QueryBuilderDialog dlg(QueryBuilderDialog::SavedQuery, this);
    dlg.builder()->setSuggestions([this](const QString &k) { return querySuggestions(k); });
    dlg.setCounter([this](const Query::Expr &e) { return countQueryMatches(e); });
    dlg.setName(tr("New Query"));
    if (dlg.exec() != QDialog::Accepted) return;
    CollectionStore::instance().create(Kind::Query, parent, dlg.name(),
                                       dlg.expr().toJsonText());
}

void MW::editQuery(qint64 id)
{
/*
    Edit a saved Query. A checked query is refiltered by the store change
    (nodesChanged -> refreshQueryNodes -> Filters::setSetNodes notices the new text).
*/
    if (G::isLogger) G::log("MW::editQuery");
    CollectionStore &store = CollectionStore::instance();
    const CollectionStore::Node n = store.node(id);
    if (n.id == 0 || n.kind != Kind::Query) return;
    QueryBuilderDialog dlg(QueryBuilderDialog::SavedQuery, this);
    dlg.builder()->setSuggestions([this](const QString &k) { return querySuggestions(k); });
    dlg.setCounter([this](const Query::Expr &e) { return countQueryMatches(e); });
    dlg.setName(n.name);
    dlg.setExpr(Query::Expr::fromJsonText(n.definition));
    if (dlg.exec() != QDialog::Accepted) return;
    store.rename(id, dlg.name());
    store.setDefinition(id, dlg.expr().toJsonText());
}

void MW::migrateSavedSearchQueries()
{
/*
    ONCE: the Search row's saved text queries (QSettings "SavedSearchQueries", from
    builds before the Queries panel) become Queries, so there is one place for saved
    searches. The settings array is left where it was -- nothing is lost if this build
    is rolled back -- and a flag stops it being imported twice.
*/
    if (settings->value("SavedSearchQueriesMigrated", false).toBool()) return;
    if (!CollectionStore::instance().isAvailable()) return;     // try again next launch
    const auto legacy = Filters::legacySavedSearchQueries();
    for (const auto &q : legacy)
        CollectionStore::instance().create(Kind::Query, 0, q.first,
                                           Query::Expr::parse(q.second).toJsonText());
    settings->setValue("SavedSearchQueriesMigrated", true);
    if (G::isLogger)
        G::log("MW::migrateSavedSearchQueries", QString::number(legacy.size()) + " moved");
}

void MW::addPathsToCollection(qint64 id, const QStringList &paths)
{
/*
    Thumbnails dropped on a collection. ONLY LIBRARY IMAGES JOIN: a collection is a set
    of catalogued images, and a file the index does not know would be a member that
    never shows. The grid's own rows are catalogued by definition (the Library is what
    is loaded); anything else -- a Finder drag -- is asked of the catalog. Sidecars ride
    along with a thumbnail drag when "include sidecars" is on, and are not images.
*/
    if (G::isLogger) G::log("MW::addPathsToCollection", QString::number(paths.size()));
    if (G::scope != G::Scope::Catalog) return;

    const QStringList &sidecar = FileOps::sidecarSuffixes();
    QStringList keep, ask;
    for (const QString &p : paths) {
        const QString suffix = QFileInfo(p).suffix().toLower();
        if (sidecar.contains(suffix, Qt::CaseInsensitive)) continue;
        if (dm->rowFromKey(p) >= 0) keep << p;
        else ask << p;
    }
    int notInLibrary = 0;
    if (!ask.isEmpty()) {
        const auto known = Catalog::instance().availabilityOf(ask);
        for (const QString &p : ask) {
            if (known.contains(p)) keep << p;
            else ++notInLibrary;
        }
    }

    const QString name = collectionTree ? collectionTree->nameOf(id) : QString();
    // membersChanged -> refreshCollectionMembership: the column, counts and a refilter
    const int added = CollectionStore::instance().addMembers(id, keep);
    QString msg;
    if (added) msg = tr("Added %n image(s) to \"%1\".", "", added).arg(name);
    else if (!keep.isEmpty()) msg = tr("Already in \"%1\".").arg(name);
    if (notInLibrary)
        msg += (msg.isEmpty() ? "" : "\n")
             + tr("%n file(s) not in the Library were not added.", "", notInLibrary);
    if (!msg.isEmpty() && G::popup) G::popup->showPopup(msg, 2000);
}

static QStringList selectedSourcePaths(DataModel *dm)
{
    QStringList paths;
    for (const QModelIndex &sfIdx : dm->selectionModel->selectedRows()) {
        const QString p = sfIdx.data(G::SourcePathRole).toString();
        if (!p.isEmpty() && !paths.contains(p)) paths << p;
    }
    return paths;
}

void MW::addSelectionToCollection(qint64 id)
{
    if (G::isLogger) G::log("MW::addSelectionToCollection");
    const QStringList paths = selectedSourcePaths(dm);
    if (paths.isEmpty()) {
        if (G::popup) G::popup->showPopup(tr("Select some thumbnails first."), 2000);
        return;
    }
    addPathsToCollection(id, paths);
}

void MW::removeSelectionFromCollection(qint64 id)
{
/*
    Take the selected images out of a collection the grid is filtered to (CollectionTree
    offers this only then). The store change refreshes the column and refilters
    (refreshCollectionMembership), so the images leave the grid unless another checked
    collection still holds them -- no reload, no picks question.
*/
    if (G::isLogger) G::log("MW::removeSelectionFromCollection");
    const QStringList paths = selectedSourcePaths(dm);
    if (paths.isEmpty()) {
        if (G::popup) G::popup->showPopup(tr("Select the thumbnails to remove first."),
                                          2000);
        return;
    }
    const QString name = collectionTree ? collectionTree->nameOf(id) : QString();
    const int removed = CollectionStore::instance().removeMembers(id, paths);
    if (G::popup)
        G::popup->showPopup(tr("Removed %n image(s) from \"%1\".", "", removed).arg(name),
                            2000);
}
