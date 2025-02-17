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
         *days;
         *models;
         *lenses;
         *focalLengths;
         *titles;
         *keywords;
         *creators;
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
        wait();
        abort = false;
    }
    if (!isReset) reset();
}

void BuildFilters::abortIfRunning()
{
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
}

void BuildFilters::rebuild()
{
    if (G::isLogger || G::isFlowLogger)
        G::log("BuildFilters::build");
    filters->filtersBuilt = false;
    filters->save();
    reset(false);
    dm->sf->suspend(false, "MW::filterChange");
    dm->sf->filterChange("MW::filterChange");
    build();
    filters->restore();
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
            << "G::allMetadataLoaded =" << G::allMetadataLoaded
               ;
    }

    // ignore if filters are up-to-date
    if (filters->filtersBuilt) return;

    // ignore if filters are being built
    if (filters->buildingFilters) return;

    if (!G::allMetadataLoaded) {
        G::popUp->showPopup("Not all data required for filtering has been loaded yet.", 2000);
        return;
    }

    // Update action to take after build filters. If build has been previously called
    // while the DataModel metadata was being loaded then the previous afterAction will
    // still be defined and should be honoured unless the new call to build has a defined
    // newAction.

    if (newAction != AfterAction::NoAfterAction) {
        afterAction = newAction;
    }

    // define action for BuildFilters::run
    action = Action::Reset;
    abortIfRunning();
    instance = dm->instance;
    filters->startBuildFilters(isReset);
    progress = 0;
    dmRows = dm->rowCount();
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
    abortIfRunning();
    if (filters->filtersBuilt) {
        action = Action::Update;
        if (G::allMetadataLoaded) start(NormalPriority);
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
    updateUnfilteredCounts();
    updateFilteredCounts();
}

void BuildFilters::updateCategory(BuildFilters::Category category, AfterAction newAction)
{
/*
    Called when a category item has been edited.  The old name is removed from the
    category items, the new one is appended and the items are resorted.  The item
    counts for this category only are updated.

    DataModel filtering (SortFilter::filterAcceptsRow) must be suspended while
    category items are being removed or appended, becasue it iterates through all
    the category items and will crash with a bad memory access when it tries to
    access a removed item.
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
    abortIfRunning();
    afterAction = newAction;
    this->category = category;
    if (filters->filtersBuilt) {
        action = Action::UpdateCategory;
        if (G::allMetadataLoaded) start(NormalPriority);
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

void BuildFilters::updateUnfilteredSearchCount()
/*
    When the search text changes then the total unfiltered that both match and do not
    match may change.  BuildFilters::update is called, which in turn, calls this function
    to update the unfiltered totals.  BuildFilters::updateFilteredCounts() is also called,
    where the unfiltered totals are updated.
*/
{
    QMap<QString,int> map;
    int rows = dm->rowCount();
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::SearchColumn).data().toString().trimmed()]++;
    }
    filters->updateSearchCategoryCount(map, false /*isFiltered*/);
}

void BuildFilters::updateUnfilteredCounts()
{
/*
    Update the DataModel item counts in Filters.  A QMap is used to count all the unique
    items for each DataModel column that can be filtered and updates the unique item counts by
    calling filters->addFilteredCountPerItem.

    This is used when images are deleted from a filtered dataset.
*/
    if (debugBuildFilters)
    {
        qDebug()
            << "BuildFilters::countMapFiltered"
            ;
    }

    QMap<QString,int> map;
    QString method = "Map";
    int rows = dm->rowCount();

    // count unfiltered
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::SearchColumn).data().toString().trimmed()]++;
    }
    //filters->updateSearchCategoryCount(map, true /*isFiltered*/);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::PickColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->picks);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::RatingColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->ratings);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::LabelColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->labels);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::TypeColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->types);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::FolderNameColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->folders);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::YearColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->years);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::DayColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->days);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::CameraModelColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->models);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::LensColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->lenses);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::FocalLengthColumn).data().toString().trimmed().rightJustified(4, ' ')]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->focalLengths);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::TitleColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->titles);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::CreatorColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->creators);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::MissingThumbColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->missingThumbs);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::CompareColumn).data().toString().trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->compare);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        QStringList x = dm->index(row, G::KeywordsColumn).data().toStringList();
        for (int i = 0; i < x.size(); i++) map[x.at(i).trimmed()]++;
    }
    filters->updateUnfilteredCountPerItem(map, filters->keywords);
    map.clear();

    filters->update();
}

void BuildFilters::updateFilteredCounts()
{
/*
    Update the filtered DataModel item counts in Filters.  A QMap is used to count all the unique
    items for each DataModel column that can be filtered and updates the unique item counts by
    calling filters->addFilteredCountPerItem.
*/
    if (debugBuildFilters)
    {
        qDebug()
            << "BuildFilters::countMapFiltered"
               ;
    }

    QMap<QString,int> map;
    QString method = "Map";
    int rows = dm->sf->rowCount();

    // count unfiltered
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::SearchColumn).data().toString().trimmed()]++;
    }
    filters->updateSearchCategoryCount(map, true /*isFiltered*/);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::PickColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->picks);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::RatingColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->ratings);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::LabelColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->labels);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::TypeColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->types);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::FolderNameColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->folders);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::YearColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->years);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::DayColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->days);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::CameraModelColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->models);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::LensColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->lenses);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::FocalLengthColumn).data().toString().trimmed().rightJustified(4, ' ')]++;
    }
    filters->updateFilteredCountPerItem(map, filters->focalLengths);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::TitleColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->titles);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::CreatorColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->creators);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::MissingThumbColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->missingThumbs);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->sf->index(row, G::CompareColumn).data().toString().trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->compare);
    map.clear();

    for (int row = 0; row < rows; row++) {
        if (abort) return;
        QStringList x = dm->sf->index(row, G::KeywordsColumn).data().toStringList();
        for (int i = 0; i < x.size(); i++) map[x.at(i).trimmed()]++;
    }
    filters->updateFilteredCountPerItem(map, filters->keywords);
    map.clear();

    filters->update();
}

void BuildFilters::updateCategoryItems()
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

    QMap<QString,int> map;
    QTreeWidgetItem *cat = nullptr;
    int col = 0;
    switch (category) {
    case Category::PickEdit:
        col = G::PickColumn;
        cat = filters->picks;
        break;
    case Category::RatingEdit:
        col = G::RatingColumn;
        cat = filters->ratings;
        break;
    case Category::LabelEdit:
        col = G::LabelColumn;
        cat = filters->labels;
        break;
    case Category::TitleEdit:
        col = G::TitleColumn;
        cat = filters->titles;
        break;
    case Category::CreatorEdit:
        col = G::CreatorColumn;
        cat = filters->creators;
        break;
    case Category::MissingThumbEdit:
        col = G::MissingThumbColumn;
        cat = filters->missingThumbs;
        break;
    case Category::CompareEdit:
        col = G::CompareColumn;
        cat = filters->compare;
        break;
    }

    for (int row = 0; row < dm->rowCount(); row++) {
        if (abort) return;
        map[dm->index(row, col).data().toString()]++;
    }

    // update filter list and unfiltered counts
    filters->updateCategoryItems(map, cat);
    map.clear();

    // update filtered counts for category
    for (int row = 0; row < dm->sf->rowCount(); row++) {
        if (abort) return;
        map[dm->sf->index(row, col).data().toString()]++;
    }
    filters->updateFilteredCountPerItem(map, cat);

    // filter
    //if (G::isFilter) emit filterChange("BuildFilters::updateCategory");
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

void BuildFilters::appendUniqueItems()
{
/*
    After a new folder, when the filter panel becomes visible, the DataModel unique
    items and counts are generated here.
*/
    if (debugBuildFilters)
    {
        qDebug()
            << "BuildFilters::initializeUniqueItems"
               ;
    }

    // general map to count unique instances (reused for each category)
    QMap<QString,int> map;
    int rows = dm->rowCount();
    double progressInc = 7.7;   // 100% / 13 categories (incl search)

    // Use QMap to count untiltered

    // search (special predefined items to always be shown)
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::SearchColumn).data().toString().trimmed()]++;
    }
    filters->updateSearchCategoryCount(map, false /*isFiltered*/);
    time("Initialize search count");
    emit updateProgress(progress += progressInc);
    map.clear();

    // picks
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::PickColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->picks);
    time("Initialize picks");
    emit updateProgress(progress += progressInc);
    map.clear();

    // ratings
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::RatingColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->ratings);
    time("Initialize ratings");
    emit updateProgress(progress += progressInc);
    map.clear();

    // labels
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::LabelColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->labels);
    time("Initialize labels");
    emit updateProgress(progress += progressInc);
    map.clear();

    // file types
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::TypeColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->types);
    time("Initialize types");
    emit updateProgress(progress += progressInc);
    map.clear();

    // folders
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::FolderNameColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->folders);
    time("Initialize folders");
    emit updateProgress(progress += progressInc);
    map.clear();

    // years
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        QString yr = dm->index(row, G::YearColumn).data().toString().trimmed();
        if (yr == "") qDebug() << "row" << row << "is blank";
        map[dm->index(row, G::YearColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->years);
    time("Initialize years");
    emit updateProgress(progress += progressInc);
    map.clear();

    // days
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::DayColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->days);
    time("Initialize days");
    emit updateProgress(progress += progressInc);
    map.clear();

    // camera models
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::CameraModelColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->models);
    time("Initialize models");
    emit updateProgress(progress += progressInc);
    map.clear();

    // lenses
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::LensColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->lenses);
    time("Initialize lenses");
    emit updateProgress(progress += progressInc);
    map.clear();

    // focal lengths
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::FocalLengthColumn).data().toString().trimmed().rightJustified(4, ' ')]++;
    }
    filters->addCategoryItems(map, filters->focalLengths);
    time("Initialize focallengths");
    emit updateProgress(progress += progressInc);
    map.clear();

    // titles
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::TitleColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->titles);
    time("Initialize titles");
    emit updateProgress(progress += progressInc);
    map.clear();

    // creators
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::CreatorColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->creators);
    time("Initialize creators");
    emit updateProgress(progress += progressInc);
    map.clear();

    // missing thumbnails
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::MissingThumbColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->missingThumbs);
    time("Initialize missing thumbs");
    emit updateProgress(progress += progressInc);
    map.clear();

    // duplicate found (compare)
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        map[dm->index(row, G::CompareColumn).data().toString().trimmed()]++;
    }
    filters->addCategoryItems(map, filters->compare);
    time("Initialize compare");
    emit updateProgress(progress += progressInc);
    map.clear();

    // keywords
    for (int row = 0; row < rows; row++) {
        if (abort) return;
        QStringList x = dm->index(row, G::KeywordsColumn).data().toStringList();
        for (int i = 0; i < x.size(); i++) map[x.at(i).trimmed()]++;
    }
    filters->addCategoryItems(map, filters->keywords);
    time("Initialize keywords");
    emit updateProgress(progress += progressInc);
    map.clear();
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

void BuildFilters::run()
{
    if (G::isLogger || G::isFlowLogger)
        G::log("BuildFilters::run", "afteraction = " + QString::number(afterAction));
    if (debugBuildFilters)
    {
        qDebug()
            << "BuildFilters::run"
            << "action =" << action
            << "filters->filtersBuilt =" << filters->filtersBuilt
               ;
    }

    if (reportTime) {
        msTot = 0;
        buildFiltersTimer.restart();
    }

    switch (action) {
    case Action::Reset:
        if (!abort /*&& filters->filtersBuilt*/) appendUniqueItems();
        break;
    case Action::Update:
        if (!abort) {
            updateFilteredCounts();
            updateUnfilteredSearchCount();
        }
        break;
    // category item edited
    case Action::UpdateCategory:
        // no change to SortFilter
        //afterAction = AfterAction::NoFilterChange;
        if (!abort) updateCategoryItems();
    }

    if (!abort) filters->setEachCatTextColor();

    done();

    /* elapsed time
    qDebug() << "BuildFilters::run"
             << buildFiltersTimer.elapsed() << "ms"
             << "Rows:" << dmRows
             << "Src:" << dm->currentFolderPath
                ;
                  //*/
}
