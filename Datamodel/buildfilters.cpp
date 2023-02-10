#include "buildfilters.h"

/*
    The DataModel is filtered based on which items are checked in the filter tree
    class Filters.  The actual filtering happens in SortFilter::filterAcceptsRow.

    Filters are based on Categories that contain items.  The categories are:

         *refine;
         *picks;
         *ratings;
         *labels;
         *types;
         *models;
         *titles;
         *lenses;
         *keywords;
         *creators;
         *focalLengths;
         *years;
         *days;

    An item is a unique value for its category in the DataModel.  For example,
    in the category types, items could include JPG, NEF, PNG ...

    BuildFilters does the following:

        • append unique items for categories (build filter tree)
        • count occurrances of items in the datamodel dm (unfiltered)
        • count occurrances of items in the proxy sf (filtered)
        • update items if values edited in DataModel

    Filters visibility

        IMPORTANT - filters cannot be edited (add and remove rows) when it is
        hidden.  The isReset flag is used to rebuild the filters tree when
        MW::filterDockVisibilityChange = visible for a new folder.

        reset() is called from MW::folderSelectionChange, setting isRest = true and
        isReset is set to false in BuildFilters::done.
*/

BuildFilters::BuildFilters(QObject *parent,
                           DataModel *dm,
                           Metadata *metadata,
                           Filters *filters,
                           bool &combineRawJpg) :
                           QThread(parent),
                           combineRawJpg(combineRawJpg)
{
    if (G::isLogger) G::log("BuildFilters::BuildFilters");
    this->dm = dm;
    this->metadata = metadata;
    this->filters = filters;
    afterAction = "";
    debugBuildFilters = true;
    reportTime = false;
}

void BuildFilters::stop()
{
    /*
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::stop"
               ; //*/
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
        filters->removeChildrenDynamicFilters();
        filters->clearAll();
        filters->filtersBuilt = false;
        filters->buildingFilters = false;
        filters->filterLabel->setVisible(false);
//        filters->bfProgressBar->setVisible(false);
//        filters->disableColorZeroCountItems();
        filters->setEnabled(true);
        filters->collapseAll();
//        emit updateIsRunning(false);
    }
    if (G::stop) emit stopped("BuildFilters");
}

/*void BuildFilters::setAfterAction(QString afterAction)
{
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::setAfterAction"
            << "afterAction =" << afterAction
               ;
    this->afterAction = afterAction;
}
*/

void BuildFilters::build(BuildFilters::Action action, QString afterAction)
{
/*
    Filters is a tree where the primary branches are categories and the leafs are the
    criteria for a filtration of the DataModel. Filters just contains the category
    headers when a new folder is selected.

    action == Reset (default):

        • All unique items for each category are appended to Filters.
        • The total and filtered counts are updated.



*/
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::build");
    if (debugBuildFilters)
    {
        qDebug()
            << "BuildFilters::build"
            << "action =" << action
            << "filters visible =" << filters->isVisible()
               ;
    }

    // update action to take after build filters
    this->afterAction = afterAction;

    // defer update if filters is hidden
    if (filters->isHidden()) {
        if (action > Update) filters->filtersBuilt = false;
        action = Reset;
        return;
    }

    // ignore if filters are up-to-date
    if (action == Reset && filters->filtersBuilt) return;

    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    this->action = action;
    instance = dm->instance;
    filters->startBuildFilters(isReset);
    progress = 0;
    dmRows = dm->rowCount();
    if (G::allMetadataLoaded) start(NormalPriority);
}

void BuildFilters::done()
{
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::done");
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::done"
               ;
    isReset = false;
    filters->filtersBuilt = true;
    emit updateProgress(-1);        // clear progress msg
    if (!abort) emit finishedBuildFilters();
    if (afterAction == "QuickFilter") emit quickFilter();
    if (afterAction == "FilterLastDay") emit filterLastDay();
    if (afterAction == "SearchTextEdit") emit searchTextEdit();
    afterAction = "";
//    qint64 msec = buildFiltersTimer.elapsed();
//    qDebug() << "BuildFilters::done" << QString("%L1").arg(msec) << "msec";
}

void BuildFilters::reset()
{
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::reset");
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::reset"
               ;
    isReset = true;
    filters->filtersBuilt = false;
    action = Action::Reset;
    afterAction = "";
    filters->activeCategory = nullptr;
    // clear all items for filters based on data content ie file types, camera model
    filters->removeChildrenDynamicFilters();
}

void BuildFilters::updateFilteredCounts()
{
/*
    Update the filtered DataModel item counts in Filters.  A QMap is used to count all the unique
    items for each DataModel column that can be filtered and updates the unique item counts by
    calling filters->addFilteredCountPerItem.
*/
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::countMapFiltered"
               ;

    QMap<QString,int> map;
    QString method = "Map";
    int rows = dm->sf->rowCount();

    // count unfiltered
    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::PickColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->picks);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::RatingColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->ratings);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::LabelColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->labels);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::TypeColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->types);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::YearColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->years);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::DayColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->days);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::CameraModelColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->models);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::LensColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->lenses);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::FocalLengthColumn).data().toString().trimmed().rightJustified(4, ' ')]++;
    filters->addFilteredCountPerItem(map, filters->focalLengths);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::TitleColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->titles);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->sf->index(row, G::CreatorColumn).data().toString().trimmed()]++;
    filters->addFilteredCountPerItem(map, filters->creators);
    map.clear();

    for (int row = 0; row < rows; row++) {
        QStringList x = dm->index(row, G::KeywordsColumn).data().toStringList();
        for (int i = 0; i < x.size(); i++) map[x.at(i).trimmed()]++;
    }
    filters->addFilteredCountPerItem(map, filters->keywords);
    map.clear();

//    filters->disableColorZeroCountItems();
//    filters->setCatFiltering();
//    filters->filtersBuilt = true;
//    filters->buildingFilters = false;
    filters->update();

//    qDebug() << "BuildFilters::countMapUnfiltered"
//             << buildFiltersTimer.elapsed() << "ms"
//             << "Method:" << method
//             << "Rows:" << dmRows
//             << "Src:" << dm->currentFolderPath
//                ;
}

void BuildFilters::updateCategoryItems()
{
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::updateCategory");
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::updateCategory"
            << "action =" << action
               ;
    QMap<QString,int> map;
    QTreeWidgetItem *cat = nullptr;
    int col = 0;
    switch (action) {
    case PickEdit:
        col = G::PickColumn;
        cat = filters->picks;
        break;
    case RatingEdit:
        col = G::RatingColumn;
        cat = filters->ratings;
        break;
    case LabelEdit:
        col = G::LabelColumn;
        cat = filters->labels;
        break;
    case TitleEdit:
        col = G::TitleColumn;
        cat = filters->titles;
        break;
    case CreatorEdit:
        col = G::CreatorColumn;
        cat = filters->creators;
        break;
    case Reset: break; // not used, prevent compiler msg
    }

    for (int row = 0; row < dm->rowCount(); row++) map[dm->index(row, col).data().toString()]++;

    // update filter list and unfiltered counts
    filters->updateCategoryItems(map, cat);
    map.clear();

    // update filtered counts for category
    for (int row = 0; row < dm->sf->rowCount(); row++) map[dm->sf->index(row, col).data().toString()]++;
    filters->addFilteredCountPerItem(map, cat);
}

void BuildFilters::appendUniqueItems()
{
/*
    After a new folder, when the filter panel becomes visible, the DataModel unique
    fitems and counts are generated here.
*/
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::initializeUniqueItems"
               ;
    QMap<QString,int> map;
    int rows = dm->rowCount();
    double progressInc = 8.5;

    // count unfiltered
    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::PickColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->picks);
    time("Initialize picks");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::RatingColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->ratings);
    time("Initialize ratings");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::LabelColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->labels);
    time("Initialize labels");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::TypeColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->types);
    time("Initialize types");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::YearColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->years);
    time("Initialize years");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::DayColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->days);
    time("Initialize days");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::CameraModelColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->models);
    time("Initialize models");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::LensColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->lenses);
    time("Initialize lenses");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::FocalLengthColumn).data().toString().trimmed().rightJustified(4, ' ')]++;
    filters->addCategoryItems(map, filters->focalLengths);
    time("Initialize focallengths");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::TitleColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->titles);
    time("Initialize titles");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++)
        map[dm->index(row, G::CreatorColumn).data().toString().trimmed()]++;
    filters->addCategoryItems(map, filters->creators);
    time("Initialize creators");
    emit updateProgress(progress += progressInc);
    map.clear();

    for (int row = 0; row < rows; row++) {
        QStringList x = dm->index(row, G::KeywordsColumn).data().toStringList();
        for (int i = 0; i < x.size(); i++) map[x.at(i).trimmed()]++;
    }
    filters->addCategoryItems(map, filters->keywords);
    time("Initialize keywords");
    emit updateProgress(progress += progressInc);
    map.clear();


//    qDebug() << "BuildFilters::countMapUnfiltered"
//             << buildFiltersTimer.elapsed() << "ms"
//             << "Method:" << method
//             << "Rows:" << dmRows
//             << "Src:" << dm->currentFolderPath
//                ;
}

void BuildFilters::time(QString msg)
{
    if (!reportTime) return;
    int ms = buildFiltersTimer.elapsed();
    totms += ms;
    msg.resize(25, ' ');
    qDebug() << "BuildFilters::time"
             << msg
             << ms << "ms"
             << totms << "total ms so far"
                ;
    buildFiltersTimer.restart();
}

void BuildFilters::run()
{
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log("BuildFilters::run"); mutex.unlock();}
    if (debugBuildFilters)
        qDebug()
            << "BuildFilters::run"
            << "action =" << action
               ;

//    if (filters->filtersBuilt) return;

    if (reportTime) {
        totms = 0;
        buildFiltersTimer.restart();
    }

    switch (action) {
    case Reset:
        if (!abort && !filters->filtersBuilt) appendUniqueItems();
        break;
    case Update:
        if (!abort) updateFilteredCounts();
        break;
    // category item edited
    default:
        updateCategoryItems();
    }

//    if (!abort) filters->disableEmptyCat();
    if (!abort) filters->setEachCatTextColor();

    done();

//    qDebug() << "BuildFilters::run"
//             << buildFiltersTimer.elapsed() << "ms"
//             << "Rows:" << dmRows
//             << "Src:" << dm->currentFolderPath
//                ;

}
