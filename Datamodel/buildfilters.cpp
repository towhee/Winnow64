#include "buildfilters.h"

/*
    Classes involved in filtering:

        DataModel                       Dataset
        SortFilter                      Proxy
        Filters                         QTreeWidget of filter items
        BuildFilters                    Update Filters
        MW - sortandfilter.cpp
        MW - pick.cpp

    DataModel and Proxies

        dm is the instance of the datamodel and includes the entire dataset.

        dm->sf is the proxy and is filtered based on which items are checked
        in the filter tree class Filters.  The actual filtering happens in
        SortFilter::filterAcceptsRow.

    Filters are based on Categories that contain items.  The categories are:

         *search
         *picks;
         *ratings;
         *labels;
         *types;
         *folders
         *years;
         *months;
         *days;
         *models;
         *lenses;
         *focalLengths;
         *isos;
         *titles;
         *keywords;
         *creators;
         *availability;
         *missingThumbs;
         *compare;

    An item is a unique value for its category in the DataModel.  For example,
    in the category "types", items could include JPG, NEF, PNG ...

    BuildFilters does the following:

        • append unique items for categories (build filter tree)
        • count occurrances of items in the datamodel dm (unfiltered)
        • count occurrances of items in the proxy sf (filtered)
        • update items if values edited in DataModel (ie ∆ rating or edit title)

    Filters visibility

        IMPORTANT - filters cannot be edited (add and remove rows) when it is
        hidden.  The isReset flag is used to rebuild the filters tree when
        MW::filterDockVisibilityChange = visible for a new folder.

        reset() is called from MW::folderSelectionChange, setting isRest = true and
        isReset is set to false in BuildFilters::done.

        Do not attempt to build filters when the filter panel is not visible, as this
        can cause a crash if there are any videos in the mix.

    Dynamic category items

        Picks, Ratings, Color classes, Titles and Creator can be edited at runtime,
        dynamically affecting sort and filter operations.

        The steps are:

            - edit the datamodel
            - update filter category items and counts: BuildFilters::updateCategory
            - if category is being filtered then MW::filterChange
            - else refresh views
            - update status bar

    Filter Change

        All filter change executions must be invoked by calling MW::filterChange to
        ensure all the followed occur:

            - the datamodel instance is incremented
            - the datamodel proxy filter is refreshed
            - the filter panel counts are updated
            - the current index is updated
            - any prior selection that is still available is set
            - the image cache is rebuilt to match the current filter
            - the thumb and grid first/last/thumbsPerPage parameters are recalculated
              and icons are loaded if necessary.
*/

BuildFilters::BuildFilters(QObject *parent,
                           DataModel *dm,
                           Metadata *metadata,
                           Filters *filters) :
                           QThread(parent)
{
/*
    Do not attempt to build filters when the filter panel is not visible, as this
    can cause a crash if there are any videos in the mix.
*/
    if (G::isLogger) G::log("BuildFilters::BuildFilters");
    this->dm = dm;
    this->metadata = metadata;

    this->filters = filters;
    afterAction = AfterAction::NoAfterAction;
    isReset = true;
    debugBuildFilters = false;
    reportTime = false;
}

void BuildFilters::stop()
{
    if (G::isLogger || G::isFlowLogger)
        G::log("BuildFilters::stop");
    /*
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::stop"
               ; //*/
    if (isRunning()) {
        qDebug() << "BuildFilters::stop  isRunning = true";
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        // wait();
        abort = false;
    }
    if (!isReset) reset();
}

void BuildFilters::abortProcessing()
{
    QString srcFun = "BuildFilters::abortProcessing";
    if (G::isLogger || G::isFlowLogger)
    {
        QString isGUI = QVariant(G::isGuiThread()).toString();
        G::log(srcFun, "starting, isGUI thread = " + isGUI);
    }

    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    // abort = false;

    /*  Apply anything the worker stashed before it exited, so a batch cannot
        land on a tree that the caller is about to reset. Callers are on the GUI
        thread; flushOps is a no-op anywhere else. */
    flushOps();

    if (G::isLogger || G::isFlowLogger)
        G::log(srcFun, "emit stopped");

    emit stopped(srcFun);
}

void BuildFilters::setIdle()
{
    QMutexLocker lock(&mutex);
    idle = true;
}

void BuildFilters::setBusy()
{
    QMutexLocker lock(&mutex);
    idle = false;
}

bool BuildFilters::isIdle()
{
    QMutexLocker lock(&mutex);
    if (G::isGuiThread()) return true;
    return idle;
}

bool BuildFilters::isBusy()
{
    QMutexLocker lock(&mutex);
    return !idle;
}

void BuildFilters::rebuild()
{
    if (G::isLogger || G::isFlowLogger)
        G::log("BuildFilters::rebuild");
    reset(false);
    build();
}

void BuildFilters::build(AfterAction newAction)
{
/*
    Filters is a tree where the primary branches are categories and the leafs are the
    criteria for a filtration of the DataModel. Filters just contains the category
    headers when a new folder is selected except for the Search category, which has
    two predefined items: the search item and the no match item.

    • All unique items for each category are appended to Filters.
    • The total and filtered counts are updated.
    • AfterAction is executed.

    AfterActions include:

    • Go to search
    • Quick filter (change of rating or label)
    • Filter on most recent day

    BuildFilters::build is called after a new folder is loaded if Filters is visible.
    If Filters is not visible BuildFilters::build is called by clicking on the filter
    tab or triggering a query from the filter menu.  This avoids building Filters until
    they are required.
*/
    if (G::isLogger || G::isFlowLogger)
        G::log("BuildFilters::build", "afteraction = " + QString::number(afterAction));
    if (debugBuildFilters)
    {
        qDebug()
            << "BuildFilters::build"
            << "afterAction =" << newAction
            << "filters visible =" << filters->isVisible()
            << "filters->filtersBuilt =" << filters->filtersBuilt
            << "filters->buildingFilters =" << filters->buildingFilters
            << "G::allMetadataAttempted =" << G::allMetadataAttempted
               ;
    }

    // ignore if filters are up-to-date
    if (filters->filtersBuilt) return;

    // ignore if filters are being built
    if (filters->buildingFilters) return;

    // if (!G::allMetadataAttempted) {
    //     G::popup->showPopup("Not all data required for filtering has been loaded yet.", 2000);
    //     G::issueDedup("Info",
    //                   "Filter build deferred — metadata still loading",
    //                   "BuildFilters::build");
    //     return;
    // }

    /* Update action to take after build filters. If build has been previously called
    while the DataModel metadata was being loaded then the previous afterAction will
    still be defined and should be honoured unless the new call to build has a defined
    newAction. */

    if (newAction != AfterAction::NoAfterAction) {
        afterAction = newAction;
    }

    // define action for BuildFilters::run
    action = Action::Reset;
    abortProcessing();
    instance = dm->instance;
    filters->startBuildFilters(isReset);
    progress = 0;
    dmRows = dm->rowCount();
    publishSnapshot();             // GUI thread: the worker reads nothing else
    start(NormalPriority);
}

void BuildFilters::update()
{
/*
    Update the filtered item counts in a separate thread.
*/
    if (G::isLogger || G::isFlowLogger) {
    QString msg = "filters->filtersBuilt = " + QVariant(filters->filtersBuilt).toString();
        G::log("BuildFilters::update", msg);
    }
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::update"
            << "filters->filtersBuilt =" << filters->filtersBuilt
               ;
    abortProcessing();
    if (filters->filtersBuilt) {
        action = Action::UpdateCounts;
        /*  A LOAD RUNNING, not "every row read" -- see SortFilter::filterAcceptsRow.
            G::allMetadataAttempted flaps false whenever the scroll-in verifier clears a
            stale row, and this then skipped the rebuild silently. */
        if (!G::isModifyingDatamodel) {
            publishSnapshot();     // GUI thread
            start(NormalPriority);
        }
    }
    else build();
}

void BuildFilters::updateAllCounts()
{
/*
    Update both the filtered and unfiltered item counts in a separate thread.  Unlike
    update() (filtered counts only), this also recomputes the unfiltered counts, which
    is required when the proxy baseline changes without a filter change - for example
    when combineRawJpg is toggled (raw+jpg pairs collapse/expand) or rows are added or
    removed.
*/
    if (G::isLogger || G::isFlowLogger) {
        QString msg = "filters->filtersBuilt = " + QVariant(filters->filtersBuilt).toString();
        G::log("BuildFilters::updateAllCounts", msg);
    }
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::updateAllCounts"
            << "filters->filtersBuilt =" << filters->filtersBuilt
               ;
    abortProcessing();
    if (filters->filtersBuilt) {
        action = Action::UpdateAllCounts;
        // Same gate, same reason as update().
        if (!G::isModifyingDatamodel) {
            publishSnapshot();     // GUI thread
            start(NormalPriority);
        }
    }
    else build();
}

void BuildFilters::recount()
{
/*
    Counts the filtered and unfiltered items without rebuilding the filters.  This is used
    when images are deleted or added to the current folder (ie remote embellish).  This
    happens in whatever thread calls this function.
*/
    /*  recount() runs INLINE in its caller (all four are on the GUI thread), so
        it takes its own snapshot rather than relying on one a start() left
        behind -- rows have usually just been added or deleted. */
    /*  recount() does NOT publish: MW::buildFiltersWhenModelReady calls
        build() and then recount() on the next line, so publishing here would
        swap the snapshot the freshly started worker is about to take. It uses
        its own object and leaves pendingSnap alone. */
    auto snapshot = makeSnapshot();
    FilterOps ops;
    updateUnfilteredCounts(*snapshot, ops);
    updateFilteredCounts(*snapshot, ops);
    dispatchOps(ops);       // inline: recount()'s callers are the GUI thread
}

void BuildFilters::updateCategory(BuildFilters::Category category, AfterAction newAction,
                                  bool runSync)
{
/*
    Called when a category item has been edited.  The old name is removed from the
    category items, the new one is appended and the items are resorted.  The item
    counts for this category only are updated.

    DataModel filtering (SortFilter::filterAcceptsRow) must be suspended while
    category items are being removed or appended, becasue it iterates through all
    the category items and will crash with a bad memory access when it tries to
    access a removed item.

    When runSync is true the rebuild runs inline on the GUI thread (run() is called
    directly) instead of on the worker thread.  Callers that immediately follow this
    with MW::filterChange must use runSync, otherwise filterChange un-suspends and
    re-runs filterAcceptsRow while the worker thread is still mutating the category
    tree items - a use-after-free crash (see MW::togglePick).
*/
    if (G::isLogger || G::isFlowLogger) qDebug() << "BuildFilters::update";
    if (debugBuildFilters)
    {
        qDebug()
                << "BuildFilters::updateCategory"
                << "category =" << category
                << "afterAction =" << newAction
                << "filters->filtersBuilt =" << filters->filtersBuilt
                   ;
    }
    dm->sf->suspend(true, "BuildFilters::update");
    abortProcessing();
    afterAction = newAction;
    this->category = category;
    if (filters->filtersBuilt) {
        action = Action::UpdateCategory;
        /*  Same gate, same reason as update(). On G::allMetadataAttempted this skipped
            the rebuild AND returned with the proxy still suspended above, so a keyword
            (or rating, or label) edited in a catalog scope updated the file and the model
            but never the filter category. */
        if (!G::isModifyingDatamodel) {
            publishSnapshot();     // GUI thread, before either route
            if (runSync) run();     // inline on GUI thread (no race with filterChange)
            else start(NormalPriority);
        }
        else {
            /*  NOTHING WILL LIFT IT. The suspension above is lifted by run() completing
                or by the caller's MW::filterChange; on this path neither happens, and a
                suspended proxy accepts every row. Callers do follow with filterChange,
                which lifts it on both of its paths -- but relying on that leaves the
                latch held by whoever forgets, which is how it was lost before. */
            dm->sf->suspend(false, "BuildFilters::updateCategory deferred");
        }
    }
    else build();
}

void BuildFilters::done()
{
    if (G::isLogger || G::isFlowLogger)
        G::log("BuildFilters::done", "afteraction = " + QString::number(afterAction));
    if (debugBuildFilters)
    {
        qDebug()
            << "BuildFilters::done"
            << "afterAction =" << afterAction
               ;
    }
    // dm->sf->suspend(false);
    filters->setEnabled(true);
    isReset = false;
    filters->filtersBuilt = true;
    emit updateProgress(-1);        // clear progress msg
    if (!abort) emit finishedBuildFilters();
    if (afterAction == AfterAction::QuickFilter) emit quickFilter();
    if (afterAction == AfterAction::MostRecentDay) emit filterLastDay();
    if (afterAction == AfterAction::Search) emit searchTextEdit();
//    if (afterAction != AfterAction::NoFilterChange)
//        dm->sf->filterChange("BuildFilters::done");
        // emit filterChange("BuildFilters::done");     // endless loop
    afterAction = AfterAction::NoAfterAction;

}

void BuildFilters::reset(bool collapse)
{
/*
    Called when a new folder is being loaded. The Filters tree dynamic category items are
    all removed, and relevent variables are reset.
*/
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::reset");
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::reset"
               ;

    filters->reset();
    afterAction = AfterAction::NoAfterAction;
    if (collapse) filters->collapseAll();
    action = Action::Reset;
    isReset = true;
}

/*  THE ONE PLACE a filter category is declared.

    Each category is a snapshot slot plus the Filters tree item its counts go
    to. appendUniqueItems, updateUnfilteredCounts and updateFilteredCounts all
    walk this list, so a category added here reaches all three at once. It used
    to be three hand-maintained copies of the same fifteen blocks.

    Keywords are NOT here -- a row carries a list of them, so they are counted
    by countKeywords and appended by each caller after this loop. */
QVector<BuildFilters::Sink> BuildFilters::sinks() const
{
    return {
        {FilterCat::Search,      filters->search,       "search"},
        {FilterCat::Pick,        filters->picks,        "picks"},
        {FilterCat::Rating,      filters->ratings,      "ratings"},
        {FilterCat::Label,       filters->labels,       "labels"},
        {FilterCat::Type,        filters->types,        "types"},
        {FilterCat::FolderName,  filters->folders,      "folders"},
        {FilterCat::Year,        filters->years,        "years"},
        {FilterCat::Month,       filters->months,       "months"},
        {FilterCat::Day,         filters->days,         "days"},
        {FilterCat::CameraModel, filters->models,       "models"},
        {FilterCat::Lens,        filters->lenses,       "lenses"},
        {FilterCat::FocalLength, filters->focalLengths, "focal lengths"},
        {FilterCat::Iso,         filters->isos,         "ISOs"},
        {FilterCat::Title,       filters->titles,       "titles"},
        {FilterCat::Creator,     filters->creators,     "creators"},
        {FilterCat::Availability, filters->availability, "availability"},
        {FilterCat::Compare,     filters->compare,      "compare"},
    };
}

std::shared_ptr<const FilterSnapshot> BuildFilters::makeSnapshot() const
{
/*
    Copy every column the counting passes need into plain data, ON THE GUI
    THREAD, so the worker never touches dm or dm->sf. See
    Datamodel/filtersnapshot.h for why.

    The proxy is walked ONCE to mark inProxy rather than asking
    sf->mapFromSource per row, which is both cheaper and the only way the
    filtered and unfiltered counts can describe the same instant.
*/
    static const int col[FilterCat::SlotCount] = {
        G::SearchColumn,      G::PickColumn,        G::RatingColumn,
        G::LabelColumn,       G::TypeColumn,        G::FolderNameColumn,
        G::YearColumn,        G::MonthColumn,       G::DayColumn,
        G::CameraModelColumn, G::LensColumn,        G::FocalLengthColumn,
        G::ISOColumn,         G::TitleColumn,       G::CreatorColumn,
        G::AvailabilityColumn, G::CompareColumn
    };

    auto out = std::make_shared<FilterSnapshot>();
    FilterSnapshot &snap = *out;
    if (dm == nullptr) return out;
    snap.instance = dm->instance;
    const int rows = dm->rowCount();
    snap.rows.resize(rows);

    const bool combine = G::combineRawJpg;
    for (int row = 0; row < rows; ++row) {
        FilterSnapshotRow &r = snap.rows[row];
        for (int slot = 0; slot < FilterCat::SlotCount; ++slot) {
            r.v[slot] = dm->index(row, col[slot]).data().toString().trimmed();
        }
        /* Focal length is right-justified so "50" and "400" sort as numbers
           rather than as text. All three passes did this, so bake it in once. */
        r.v[FilterCat::FocalLength] =
            r.v[FilterCat::FocalLength].rightJustified(4, ' ');
        /*  ISO for the same reason, to six: unpadded, "800" sorts between "100" and
            "8000". Six covers the largest ISO a camera reports (409600). The model
            holds an int, and a QVariant int compares equal to its padded string form,
            so a checked item still matches the row -- which is what lets the item
            carry the padded text as its value the way focal length does. */
        r.v[FilterCat::Iso] = r.v[FilterCat::Iso].rightJustified(6, ' ');

        /*  Availability is the one column whose value is a CODE, not a word -- see
            Catalog::Availability, and "Present is 0 and unset reads as 0", which is
            what makes a folder-scope row (nothing ever wrote this column) come out
            Present rather than blank. The category shows the word. */
        r.v[FilterCat::Availability] =
            Catalog::availabilityLabel(r.v[FilterCat::Availability].toInt());

        const QStringList kw =
            dm->index(row, G::KeywordsAllColumn).data().toStringList();
        r.keywords.reserve(kw.size());
        for (const QString &k : kw) r.keywords << k.trimmed();

        /* When combineRawJpg is on the raw half of a pair is hidden in the
           proxy (SortFilter::filterAcceptsRow), so the unfiltered totals must
           skip it or they will not match the proxy baseline. */
        r.hiddenRaw = combine &&
                      dm->index(row, 0).data(G::DupHideRawRole).toBool();
    }

    // mark the rows the current filter admits
    if (dm->sf != nullptr) {
        const int sfRows = dm->sf->rowCount();
        for (int sfRow = 0; sfRow < sfRows; ++sfRow) {
            const int dmRow = dm->sf->mapToSource(dm->sf->index(sfRow, 0)).row();
            if (dmRow >= 0 && dmRow < rows) {
                snap.rows[dmRow].inProxy = true;
                ++snap.proxyRows;
            }
        }
    }
    return out;
}

/*  Build a snapshot and hand it to the next run(). GUI thread only. */
void BuildFilters::publishSnapshot()
{
    auto out = makeSnapshot();
    QMutexLocker lock(&mutex);
    pendingSnap = out;
}

QMap<QString,int> BuildFilters::countSlot(const FilterSnapshot &snap, int slot,
                                         bool filtered) const
{
/*
    Count one category over the snapshot. "filtered" counts the rows the proxy
    admits; unfiltered counts every row except the hidden raw half of a
    raw+jpg pair.
*/
    QMap<QString,int> map;
    for (const FilterSnapshotRow &r : snap.rows) {
        if (abort) return map;
        if (filtered ? !r.inProxy : r.hiddenRaw) continue;
        map[r.v[slot]]++;
    }
    return map;
}

QMap<QString,int> BuildFilters::countKeywords(const FilterSnapshot &snap,
                                             bool filtered) const
{
    QMap<QString,int> map;
    for (const FilterSnapshotRow &r : snap.rows) {
        if (abort) return map;
        if (filtered ? !r.inProxy : r.hiddenRaw) continue;
        for (const QString &k : r.keywords) map[k]++;
    }
    return map;
}

void BuildFilters::updateUnfilteredSearchCount(const FilterSnapshot &snap, FilterOps &ops)
/*
    When the search text changes then the total unfiltered that both match and do not
    match may change. BuildFilters::update is called, which in turn, calls this function
    to update the unfiltered totals. BuildFilters::updateFilteredCounts() is also called,
    where the filtered totals are updated.
*/
{
    ops.append({FilterOp::SearchCount, countSlot(snap, FilterCat::Search, false),
                nullptr, false, QString()});
}

void BuildFilters::updateUnfilteredCounts(const FilterSnapshot &snap, FilterOps &ops)
{
/*
    Update the DataModel item counts in Filters from the snapshot. Used when
    images are deleted from a filtered dataset, or when the proxy baseline
    changes without a filter change (combineRawJpg toggled, rows added).

    The hidden raw member of each raw+jpg pair is skipped so the unfiltered
    totals match the proxy baseline.
*/
    if (debugBuildFilters) qDebug() << "BuildFilters::updateUnfilteredCounts";

    for (const Sink &s : sinks()) {
        if (abort) return;
        /* Search is NOT counted here. It goes through updateSearchCategoryCount
           in updateUnfilteredSearchCount(), which run() calls alongside this --
           the search category has predefined items and its own sink. The
           original code built the search map here and then threw it away. */
        if (s.slot == FilterCat::Search) continue;
        ops.append({FilterOp::UnfilteredCount, countSlot(snap, s.slot, false),
                    s.item, false, QString()});
    }
    if (abort) return;
    ops.append({FilterOp::UnfilteredCount, countKeywords(snap, false),
                filters->keywords, false, QString()});

    ops.append({FilterOp::Update, {}, nullptr, false, QString()});
}

void BuildFilters::updateFilteredCounts(const FilterSnapshot &snap, FilterOps &ops)
{
/*
    Update the filtered item counts in Filters from the snapshot -- the rows the
    proxy currently admits.
*/
    if (debugBuildFilters) qDebug() << "BuildFilters::updateFilteredCounts";

    ops.append({FilterOp::SearchCount, countSlot(snap, FilterCat::Search, true),
                nullptr, true, QString()});

    for (const Sink &s : sinks()) {
        if (abort) return;
        if (s.slot == FilterCat::Search) continue;   // handled above
        ops.append({FilterOp::FilteredCount, countSlot(snap, s.slot, true),
                    s.item, false, QString()});
    }
    if (abort) return;
    ops.append({FilterOp::FilteredCount, countKeywords(snap, true),
                filters->keywords, false, QString()});

    ops.append({FilterOp::Update, {}, nullptr, false, QString()});
    ops.append({FilterOp::MenuUpdate, {}, nullptr, false,
                "BuildFilters::updateFilterCounts"});
}

void BuildFilters::updateCategoryItems(const FilterSnapshot &snap, FilterOps &ops)
{
/*
    Called when a category item has been edited.  The old name is removed from the
    category items, the new one is appended and the items are resorted.  The item
    counts for this category only are updated.

    Example:
    BuildFilters::updateCategory(category, BuildFilters::NoAfterAction)
    Run
    BuildFilters::updateCategoryItems
    BuildFilters::done

    Call with BuildFilters::NoFilterChange to prevent a SortFilter update.
*/
    if (G::isLogger || G::isFlowLogger) qDebug() << "BuildFilters::updateCategory";
    if (debugBuildFilters)
    {
        qDebug()
            << "BuildFilters::updateCategory"
            << "action =" << action
               ;
    }

    /*  Map the edited category onto its snapshot slot and its tree item. The
        counts come from the snapshot like every other pass, so an edited value
        is keyed the same way the category list keyed it -- this used to read
        the model directly and WITHOUT trimming, so a value with stray
        whitespace could appear as a second, near-identical item. */
    int slot = -1;
    QTreeWidgetItem *cat = nullptr;
    switch (category) {
    case Category::PickEdit:    slot = FilterCat::Pick;    cat = filters->picks;    break;
    case Category::RatingEdit:  slot = FilterCat::Rating;  cat = filters->ratings;  break;
    case Category::LabelEdit:   slot = FilterCat::Label;   cat = filters->labels;   break;
    case Category::TitleEdit:   slot = FilterCat::Title;   cat = filters->titles;   break;
    case Category::CreatorEdit: slot = FilterCat::Creator; cat = filters->creators; break;
    case Category::CompareEdit: slot = FilterCat::Compare; cat = filters->compare;  break;
    case Category::AvailabilityEdit:
        slot = FilterCat::Availability; cat = filters->availability; break;
    case Category::KeywordEdit:
        /*  No slot: keywords are the one category counted from the row's LIST rather
            than from a single column, so the ops are appended here and this returns. */
        ops.append({FilterOp::CategoryItems, countKeywords(snap, false),
                    filters->keywords, false, QString()});
        if (abort) return;
        ops.append({FilterOp::FilteredCount, countKeywords(snap, true),
                    filters->keywords, false, QString()});
        return;
    case Category::MissingThumbEdit:
        // no snapshot slot: the MissingThumb category is not built (see sinks())
        return;
    }
    if (slot < 0 || cat == nullptr) return;

    // update filter list and unfiltered counts
    ops.append({FilterOp::CategoryItems, countSlot(snap, slot, false),
                cat, false, QString()});
    if (abort) return;

    // update filtered counts for category
    ops.append({FilterOp::FilteredCount, countSlot(snap, slot, true),
                cat, false, QString()});

    ops.append({FilterOp::MenuUpdate, {}, nullptr, false,
                "BuildFilters::updateCategory"});
}

void BuildFilters::updateZeroCountCheckedItems(QTreeWidgetItem *cat, int dmColumn)
{
/*
    If a category item is checked, so the datamodel is being filtered on this item, and then
    all the datamodel rows with the item value are changed, then the updated proxy filter will
    be null because the filter item is still checked.

    For example:
        - make some picks
        - filter on picked (check filter pick item)
        - select all
        - unpick
    This results in the picked filter item still being checked, but no longer visible, and a
    null proxy filter.

    This function iterates through all checked items in the category, and if the datamodel
    count for the item is zero, unchecks the item.

    This function should be called before invoking MW::filterChange().
    This function executes in the gui thread.
*/
    if (debugBuildFilters)
    {
        qDebug() << "BuildFilters::zeroCountCheckedItems";
    }

    QMap<QString,int> map;
    int rows = dm->sf->rowCount();

    // get datamodel filtered counts for category (ie picks)
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::PickColumn).data().toString().trimmed()]++;
    }

    filters->updateZeroCountCheckedItems(map, cat);
}

//void BuildFilters::updateCategoryItems(QTreeWidgetItem *category, int dmColumn)
//{
///*
//    Dynamically update the filter if items in a category have by changed in the datamodel;
//    For example, if visCmpImages updates the datamodel colum "compare" then the compare
//    filter, which may have only contained the item = false, could now also have item = true.

//    Example:
//    buildFilters->updateCategoryItems(filters->compare, G::CompareColumn);
//    filterChange();     // update filter counts
//*/
//    QMap<QString,int> map;
//    for (int row = 0; row < dm->rowCount(); row++)
//        map[dm->index(row, dmColumn).data().toString().trimmed()]++;
//    qDebug() << "BuildFilters::updateCategoryItems  map =" << map;
//    filters->addCategoryItems(map, category);
//}

void BuildFilters::appendUniqueItems(const FilterSnapshot &snap, FilterOps &ops)
{
/*
    After a new folder, when the filter panel becomes visible, the DataModel unique
    items and counts are generated here from the snapshot.
*/
    if (debugBuildFilters) qDebug() << "BuildFilters::appendUniqueItems";

    const QVector<Sink> sink = sinks();
    // 100% spread over the categories plus keywords
    const double progressInc = 100.0 / (sink.size() + 1);

    // search carries predefined items that are always shown, so it has its own sink
    ops.append({FilterOp::SearchCount, countSlot(snap, FilterCat::Search, false),
                nullptr, false, QString()});
    time("Initialize search count");
    emit updateProgress(progress += progressInc);

    for (const Sink &s : sink) {
        if (abort) return;
        if (s.slot == FilterCat::Search) continue;      // handled above
        ops.append({FilterOp::AddItems, countSlot(snap, s.slot, false),
                    s.item, false, QString()});
        time(QString("Initialize %1").arg(s.name));
        emit updateProgress(progress += progressInc);
    }

    if (abort) return;
    ops.append({FilterOp::AddItems, countKeywords(snap, false),
                filters->keywords, false, QString()});
    time("Initialize keywords");
    emit updateProgress(progress += progressInc);
}

void BuildFilters::time(QString msg)
{
    if (!reportTime) return;
    int ms = buildFiltersTimer.elapsed();
    msTot += ms;
    msg.resize(25, ' ');
    qDebug() << "BuildFilters::time"
             << msg
             << ms << "ms"
             << msTot << "total ms so far"
                ;
    buildFiltersTimer.restart();
}

/*  HAND THE RECORDED OPS TO THE GUI THREAD.

    Inline when the caller already IS the GUI thread -- updateCategory(runSync)
    calls run() directly precisely so the tree is updated BEFORE the caller goes
    on to MW::filterChange, and deferring it there would reintroduce the
    use-after-free that runSync exists to prevent.

    Otherwise the batch is stashed and a queued call posted. It is NOT a
    BlockingQueuedConnection: abortProcessing() calls wait() from the GUI
    thread, so a worker blocking on the GUI thread would deadlock against it.
*/
void BuildFilters::dispatchOps(FilterOps &ops)
{
    if (ops.isEmpty()) return;

    if (G::isGuiThread()) {
        applyOps(ops);
        return;
    }
    {
        QMutexLocker lock(&mutex);
        pendingOps += ops;
    }
    QMetaObject::invokeMethod(this, [this]() { flushOps(); }, Qt::QueuedConnection);
}

/*  Drain whatever the worker stashed. Runs on the GUI thread, either from the
    posted call above or synchronously from abortProcessing(), which drains
    before letting a new build start so a batch cannot be applied to a tree that
    has since been reset. Swapping under the mutex makes a double call harmless.
*/
void BuildFilters::flushOps()
{
    if (!G::isGuiThread()) return;
    FilterOps ops;
    {
        QMutexLocker lock(&mutex);
        ops.swap(pendingOps);
    }
    applyOps(ops);
}

void BuildFilters::applyOps(const FilterOps &ops)
{
    /*  TIMED UNDER G::isPerfProbe. Applying ops creates QTreeWidgetItems and updates
        counts on the GUI thread, once per category and once per item; at 43,000 rows that
        is the shape that already produced one 56-second stall (see "The Filter Tree
        Recompiled Once Per Item"). */
    const bool probeBig = G::isPerfProbe;
    QElapsedTimer aoTimer;
    if (probeBig) aoTimer.start();

    /*  Every branch below mutates the Filters QTreeWidget or its items. Both
        callers already check, but the guard is what keeps the invariant true
        if a third one is ever added -- this whole class exists because those
        calls used to run on the worker thread. */
    if (!G::isGuiThread()) {
        G::issue("Warning", "applyOps called off the GUI thread",
                 "BuildFilters::applyOps");
        return;
    }

    for (const FilterOp &op : ops) {
        switch (op.kind) {
        case FilterOp::SearchCount:
            filters->updateSearchCategoryCount(op.map, op.flag);
            break;
        case FilterOp::AddItems:
            filters->addCategoryItems(op.map, op.item);
            break;
        case FilterOp::UnfilteredCount:
            filters->updateUnfilteredCountPerItem(op.map, op.item);
            break;
        case FilterOp::FilteredCount:
            filters->updateFilteredCountPerItem(op.map, op.item);
            break;
        case FilterOp::CategoryItems:
            filters->updateCategoryItems(op.map, op.item);
            break;
        case FilterOp::Update:
            filters->update();
            break;
        case FilterOp::TextColor:
            filters->setEachCatTextColor();
            break;
        case FilterOp::MenuUpdate:
            emit updateFilterMenu(op.src);
            break;
        case FilterOp::Done:
            done();
            break;
        }
    }
    if (probeBig)
        qDebug().noquote() << "[PERF] BuildFilters::applyOps" << aoTimer.elapsed()
                           << "ms  ops =" << ops.size();

}

void BuildFilters::run()
{
    idle = false;
    abort = false;

    /*
        THE ACTION THIS RUN WAS ASKED FOR, CAPTURED ONCE.

        `action` is a plain member set on the GUI THREAD and read here on the worker.
        BuildFilters::updateCategory sets it to UpdateCategory immediately after calling
        abortProcessing(), so a full Reset already in flight has the value describing it
        REPLACED underneath it -- and then decides what to do, and reports what it did,
        against somebody else's request. That is what made the first attempt at the
        aborted-build recovery below do nothing: by the time run() reached its check the
        member said UpdateCategory, so the Reset that had just cleared the tree and been
        cut short never reported itself.

        Reading it once is also the fix for the switch: a run should carry out the request
        it started on, not whichever request happens to have arrived by the time it
        reaches a given line.
    */
    const Action ranAction = action;
    if (G::isLogger || G::isFlowLogger)
        G::log("BuildFilters::run", "afteraction = " + QString::number(afterAction));
    if (debugBuildFilters)
    {
        /* Deliberately does NOT print filters->filtersBuilt: run() touches
           nothing on the Filters widget, and a debug line is not a reason to
           make that untrue. */
        qDebug()
            << "BuildFilters::run"
            << "action =" << action
               ;
    }

    if (reportTime) {
        msTot = 0;
        buildFiltersTimer.restart();
    }

    /*  The counting is the only part that belongs on this thread. It reads the
        snapshot and RECORDS what the Filters tree should be told; the tree
        itself is touched on the GUI thread by applyOps. */
    /*  Take our own reference to the snapshot the entry point published. Held
        for the whole run, so a snapshot taken on the GUI thread meanwhile
        builds a separate object and cannot pull this one out from under us. */
    std::shared_ptr<const FilterSnapshot> snapshot;
    {
        QMutexLocker lock(&mutex);
        snapshot = pendingSnap;
    }
    if (!snapshot) snapshot = std::make_shared<FilterSnapshot>();

    /*
        AN EMPTY SNAPSHOT MUST NOT EMPTY THE CATEGORIES, and until now it did.

        A category is rebuilt from the counts this run produces: updateCategoryItems
        removes every item the count map does not mention, and updateKeywordItems deletes
        the whole keyword subtree and remakes it from the map. So a snapshot with NO ROWS
        does not mean "nothing to update", it means "delete everything" -- the Filters
        panel comes back with Titles, Keywords and Creators empty and greyed by
        setEachCatTextColor's childCount() == 0 test, and nothing repairs it until the
        user switches scope. That was reported three times.

        A SNAPSHOT CAN BE EMPTY WHILE THE MODEL IS NOT. makeSnapshot returns an empty one
        when dm is null, and reads dm->rowCount() at the instant it runs -- and
        DataModel::addCatalogRows fills the model in BATCHES from pendingCatalogRows, so
        there is a window in every catalog load where the model has few rows or none.
        Anything that rebuilds a category inside that window used to wipe it.

        THE TEST IS THE MODEL, NOT THE SNAPSHOT. A folder that genuinely holds no images
        must still clear the tree -- that is a real empty, and a Reset on it is correct.
        What cannot be right is a snapshot claiming zero rows while the datamodel holds
        thousands, and that is the only case skipped here.

        SKIPPING LEAVES THE TREE STALE, WHICH IS THE RIGHT FAILURE: stale is recoverable
        and self-correcting -- the next rebuild replaces it -- where empty is neither.
    */
    if (snapshot->rows.isEmpty() && dm != nullptr && dm->rowCount() > 0) {
        G::issue("Warning",
                 QString("Filter rebuild skipped: snapshot held 0 rows but the datamodel "
                         "holds %1 (action %2). The category tree was left as it was.")
                     .arg(dm->rowCount()).arg(static_cast<int>(action)),
                 "BuildFilters::run");
        /*  Finished the way an ordinary run finishes -- the Done op and the same two
            lines -- so nothing downstream is left waiting on a build that decided not to
            build. The proxy suspension taken in updateCategory is lifted by the caller's
            filterChange, exactly as on the normal path. */
        FilterOps done;
        done.append({FilterOp::Done, {}, nullptr, false, QString()});
        dispatchOps(done);
        setIdle();
        emit stopped("BuildFilters");
        return;
    }

    const FilterSnapshot &snap = *snapshot;

    FilterOps ops;

    switch (ranAction) {
    case Action::Reset:
        if (!abort) appendUniqueItems(snap, ops);
        [[fallthrough]]; // deliberate fall-through
    case Action::UpdateCounts:
        if (!abort) {
            updateFilteredCounts(snap, ops);
            updateUnfilteredSearchCount(snap, ops);
        }
        break;
    // proxy baseline changed (combineRawJpg toggle, rows added/removed)
    case Action::UpdateAllCounts:
        if (!abort) {
            updateUnfilteredCounts(snap, ops);
            updateUnfilteredSearchCount(snap, ops);
            updateFilteredCounts(snap, ops);
        }
        break;
    // category item edited
    case Action::UpdateCategory:
        // no change to SortFilter
        //afterAction = AfterAction::NoFilterChange;
        if (!abort) updateCategoryItems(snap, ops);
        break;
    }

    if (!abort) ops.append({FilterOp::TextColor, {}, nullptr, false, QString()});

    /*  done() closes the build (re-enables the panel, sets filtersBuilt, fires
        the afterAction). It runs LAST, on the GUI thread, after the tree has
        actually been updated -- it used to run here, before the GUI had seen
        anything, which is why finishedBuildFilters could reach MW while the
        panel still held the previous folder's items. */
    ops.append({FilterOp::Done, {}, nullptr, false, QString()});

    dispatchOps(ops);

    setIdle();

    /*
        AN ABORTED RESET LEAVES THE TREE CLEARED AND ONLY PARTLY REFILLED, SO IT MUST RUN
        AGAIN.

        BuildFilters::build calls Filters::startBuildFilters, which calls
        removeChildrenDynamicFilters -- every dynamic category is emptied BEFORE the
        refill starts. The refill then walks the categories in creation order and checks
        `abort` as it goes, and abortProcessing() is called at the top of every
        updateCategory. So anything that asks for a category update while a full build is
        in flight stops it partway, and the categories after that point stay empty: on the
        reported failure they were ISO, Titles, Keywords, Creators, Availability and
        Duplicates -- exactly the tail of createDynamicFilters, with everything up to
        Focal lengths intact.

        NOTHING PUT THEM BACK. The update that did the aborting fills its OWN category and
        returns, so the rest stayed empty until the user switched scope. The panel showed
        them greyed, which reads as "disabled" and was reported three times as such.

        THE ABORT ITSELF IS CORRECT -- a newer request supersedes an older one. What is
        wrong is treating a half-cleared tree as a resting place, so the abort is reported
        and the build re-run. Not from here: this runs on the worker thread, and the
        restart has to happen on the GUI thread once the churn that caused the abort has
        settled.
    */
    if (abort && ranAction == Action::Reset) emit abortedAfterClearing();

    emit stopped("BuildFilters");

    /* elapsed time
    qDebug() << "BuildFilters::run"
             << buildFiltersTimer.elapsed() << "ms"
             << "Rows:" << dmRows
             << "Src:" << dm->currentFolderPath
                ;
                  //*/
}
