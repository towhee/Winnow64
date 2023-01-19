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

        • check and load all metadata if necessary
        • add unique items for categories (build filter tree)
        • count occurrances of items in the proxy sf (filtered)
        • count occurrances of items in the datamodel dm (unfiltered)

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
}

void BuildFilters::stop()
{
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
        filters->bfProgressBar->setVisible(false);
        filters->disableColorZeroCountItems();
        filters->setEnabled(true);
        filters->collapseAll();
//        emit updateIsRunning(false);
    }
    if (G::stop) emit stopped("BuildFilters");
}

void BuildFilters::setAfterAction(QString afterAction)
{
    this->afterAction = afterAction;
}

void BuildFilters::build()
{
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::build");
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    instance = dm->instance;
    filters->startBuildFilters(isReset);
    progress = 0;
    dmRows = dm->rowCount();
    buildFiltersTimer.restart();
    start(NormalPriority);
}

void BuildFilters::done()
{
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::done");
    isReset = false;
    if (!abort) emit finishedBuildFilters();
    if (afterAction == "QuickFilter") emit quickFilter();
    afterAction = "";
//    qint64 msec = buildFiltersTimer.elapsed();
//    qDebug() << "BuildFilters::done" << QString("%L1").arg(msec) << "msec";
}

void BuildFilters::reset()
{
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::reset");
    isReset = true;
    filters->cjf = nullptr;
}

void BuildFilters::unfilteredItemSearchCount()
{
/*
    This function counts the number of occurences of each unique item in the datamodel, and
    also if raw+jpg have been combined. The results as saved in the filters QTreeWidget in
    columns 3 (raw+jpg) and 4 (all).

    This function is run everytime the search string changes.
*/
    if (G::isLogger || G::isFlowLogger) G::log("BuildFilters::unfilteredItemSearchCount");
    int col = G::SearchColumn;

    // get total matches for searchTrue
    int tot = 0;
    int totRawJpgCombined = 0;
    QString matchText = filters->searchTrue->text(1);
    for (int row = 0; row < dmRows; ++row) {
        bool hideRaw = dm->index(row, 0).data(G::DupHideRawRole).toBool();
            if (dm->index(row, col).data().toString() == matchText) {
                tot++;
                if (combineRawJpg && !hideRaw) totRawJpgCombined++;
            }
    }
    filters->searchTrue->setData(3, Qt::EditRole, QString::number(tot));
    filters->searchTrue->setData(4, Qt::EditRole, QString::number(totRawJpgCombined));

    // get total matches for searchFalse
    tot = 0;
    totRawJpgCombined = 0;
    matchText = filters->searchFalse->text(1);
    dm->mutex.lock();
    for (int row = 0; row < dmRows; ++row) {
        bool hideRaw = dm->index(row, 0).data(G::DupHideRawRole).toBool();
        if (dm->index(row, col).data().toString() == matchText) {
            tot++;
            if (combineRawJpg && !hideRaw) totRawJpgCombined++;
        }
    }
    dm->mutex.unlock();
    filters->searchFalse->setData(3, Qt::EditRole, QString::number(totRawJpgCombined));
    filters->searchFalse->setData(4, Qt::EditRole, QString::number(tot));
}

void BuildFilters::updateCountFiltered()
{
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log("BuildFilters::updateCountFiltered"); mutex.unlock();}
    filters->filtersBuilt = false;
    filters->buildingFilters = true;
    QTreeWidgetItemIterator it(filters);
    QString cat = "";    // category ie Search, Ratings, Labels, etc
    while (*it) {
        if ((*it)->parent()) {
            if (abort) return;
            cat = (*it)->parent()->text(0);
            int col = filters->filterCategoryToDmColumn[cat];
            QString searchValue = (*it)->text(1);
            int tot = 0;
            // filtered counts
            dm->mutex.lock();
            for (int row = 0; row < dm->sf->rowCount(); ++row) {
                if (cat == " Keywords") {
                    QStringList x = dm->sf->index(row, col).data().toStringList();
                    for (int i = 0; i < x.size(); i++) {
                        if (x.at(i) == searchValue) tot++;
                    }
                }
                else {
                    if (dm->sf->index(row, col).data().toString() == searchValue) tot++;
                }
            }
            dm->mutex.unlock();
            (*it)->setData(2, Qt::EditRole, QString::number(tot));
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
            // unfiltered counts
            tot = 0;
            for (int row = 0; row < dm->rowCount(); ++row) {
                if (cat == " Keywords") {
                    QStringList x = dm->index(row, col).data().toStringList();
                    for (int i = 0; i < x.size(); i++) {
                        if (x.at(i) == searchValue) tot++;
                    }
                }
                else {
                    if (dm->index(row, col).data().toString() == searchValue) tot++;
                }
            }
            (*it)->setData(4, Qt::EditRole, QString::number(tot));
            (*it)->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
        }
        ++it;
    }
    filters->disableColorZeroCountItems();
    filters->setCatFiltering();
    filters->filtersBuilt = true;
    filters->buildingFilters = false;
    filters->update();
}

void BuildFilters::countFiltered()
{
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log("BuildFilters::countFiltered"); mutex.unlock();}
    // count filtered
    QTreeWidgetItemIterator it(filters);
    QString cat = "";    // category ie Search, Ratings, Labels, etc
    while (*it) {
        if ((*it)->parent()) {
            if (abort) return;
            cat = (*it)->parent()->text(0);
            int col = filters->filterCategoryToDmColumn[cat];
            QString searchValue = (*it)->text(1);
            int tot = 0;
            dm->mutex.lock();
            for (int row = 0; row < dm->sf->rowCount(); ++row) {
                if (cat == "Keywords") {
                    QStringList x = dm->index(row, col).data().toStringList();
                    for (int i = 0; i < x.size(); i++) {
                        if (x.at(i) == searchValue) tot++;
                    }
                }
                else {
                    /*
                    if (cat == "Picks")
                    qDebug() << "BuildFilters::countFiltered"  << cat
                             << "row = " << row
                             << "col =" << col
                             << "dm val =" << dm->sf->index(row, col).data().toString()
                             << "searchValue =" << searchValue
                                ;
                                //*/
                    if (dm->sf->index(row, col).data().toString() == searchValue) tot++;
                }
            }
            dm->mutex.unlock();
            (*it)->setData(2, Qt::EditRole, QString::number(tot));
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        }
        if (!(*it)->parent()) {
            /*
            qDebug() << "BuildFilters::countFiltered  cat =" << cat;
            //*/
            if (uniqueItemCount.contains(cat)) {
                int itemProgress = 40 * uniqueItemCount[cat] / totUniqueItems;
                progress += itemProgress;
                emit updateProgress(progress);
                /*
                qDebug() << "BuildFilters::countFiltered"
                         << cat
                         << "instances =" << instances[cat]
                         << "total instances =" << totInstances
                         << "itemProgress % =" << itemProgress
                         << "Progress % =" << progress;
              //*/
            }
        }
        ++it;
    }
    filters->disableColorZeroCountItems();
    progress = 60;
}

void BuildFilters::countUnfiltered()
{
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log("BuildFilters::countUnfiltered"); mutex.unlock();}
    // count unfiltered
    QTreeWidgetItemIterator it(filters);
    QString cat = "";    // category ie Search, Ratings, Labels, etc
    // iterate all items in filters tree
    while (*it) {
        // items
        if ((*it)->parent()) {
            if (abort) return;
            cat = (*it)->parent()->text(0);
            int col = filters->filterCategoryToDmColumn[cat];
            QString searchValue = (*it)->text(1);
            int tot = 0;
            int totRawJpgCombined = 0;
            dm->mutex.lock();
            for (int row = 0; row < dmRows; ++row) {
                bool hideRaw = dm->index(row, 0).data(G::DupHideRawRole).toBool();
                if (cat == "Keywords") {
                    QStringList x = dm->index(row, col).data().toStringList();
                    for (int i = 0; i < x.size(); i++) {
                        if (x.at(i) == searchValue) {
                            tot++;
                            if (combineRawJpg && !hideRaw) totRawJpgCombined++;
                        }
                    }
                }
                else {
                    if (dm->index(row, col).data().toString() == searchValue) {
                        tot++;
                        if (combineRawJpg && !hideRaw) totRawJpgCombined++;
                    }
                }
            }
            dm->mutex.unlock();
            (*it)->setData(3, Qt::EditRole, QString::number(totRawJpgCombined));
            (*it)->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
            (*it)->setData(4, Qt::EditRole, QString::number(tot));
            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        }
        // categories
        if (!(*it)->parent()) {
            /*
            qDebug() << "BuildFilters::countUnfiltered Category =" << cat;
            //*/
            if (uniqueItemCount.contains(cat)) {
                int itemProgress = 40 * uniqueItemCount[cat] / totUniqueItems;
                progress += itemProgress;
                emit updateProgress(progress);
                /*
                qDebug() << "BuildFilters::countUnfiltered"
                         << cat
                         << "instances =" << uniqueItemCount[cat]
                         << "total instances =" << totUniqueItems
                         << "itemProgress % =" << itemProgress
                         << "Progress % =" << progress;
                         //*/
            }
        }
        ++it;
    }
    progress = 100;
}

void BuildFilters::loadAllMetadata()
{
/*
    Load all metadata if not already loaded.  Cannot filter without all the data.
*/
    QString src = "BuildFilters::loadAllMetadata";
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log(src); mutex.unlock();}
    if (!G::allMetadataLoaded) {
        for (int row = 0; row < dmRows; ++row) {
            if (abort) return;
            dm->mutex.lock();
            // is metadata already cached
            if (dm->index(row, G::MetadataLoadedColumn).data().toBool()) continue;
            QString fPath = dm->index(row, 0).data(G::PathRole).toString();
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, src)) {
                metadata->m.row = row;
                metadata->m.instance = instance;
                emit addToDatamodel(metadata->m, "BuildFilters::loadAllMetadata");
                if (row % 100 == 0 || row == 0) {
                    progress = static_cast<int>(static_cast<double>(20 * row) / dmRows);
                    emit updateProgress(progress);
                }
            }
            dm->mutex.unlock();
        }
        G::allMetadataLoaded = true;
    }
    progress = 20;
}

void BuildFilters::mapUniqueInstances()
{
/*

*/
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log("BuildFilters::mapUniqueInstances"); mutex.unlock();}
    // collect all unique instances for filtration (use QMap to maintain order)
    QStringList refineList;
    QStringList pickList;
    QStringList ratingList;
    QStringList labelList;
    QStringList typeList;
    QStringList modelList;
    QStringList lensList;
    QStringList titleList;
    QStringList keywordList;
    QStringList flList;
    QStringList creatorList;
    QStringList yearList;
    QStringList dayList;
    for (int row = 0; row < dm->sf->rowCount(); row++) {
        if (abort) return;
//        QString refine = dm->sf->index(row, G::RefineColumn).data().toString();
//        if (!refineList.contains(refine)) refineList.append(refine);
        QString pick = dm->sf->index(row, G::PickColumn).data().toString();
        if (!pickList.contains(pick)) pickList.append(pick);
        QString rating = dm->sf->index(row, G::RatingColumn).data().toString();
        if (!ratingList.contains(rating)) ratingList.append(rating);
        QString label = dm->sf->index(row, G::LabelColumn).data().toString();
        if (!labelList.contains(label)) labelList.append(label);
        QString type = dm->sf->index(row, G::TypeColumn).data().toString();
        if (!typeList.contains(type)) typeList.append(type);
        QString model = dm->sf->index(row, G::CameraModelColumn).data().toString();
        if (!modelList.contains(model)) modelList.append(model);
        QString lens = dm->sf->index(row, G::LensColumn).data().toString();
        if (!lensList.contains(lens)) lensList.append(lens);
        QString title = dm->sf->index(row, G::TitleColumn).data().toString();
        if (!titleList.contains(title)) titleList.append(title);
        QString flNum = dm->sf->index(row, G::FocalLengthColumn).data().toString();
        if (!flList.contains(flNum)) flList.append(flNum);
        QString creator = dm->sf->index(row, G::CreatorColumn).data().toString();
        if (!creatorList.contains(creator)) creatorList.append(creator);
        QString year = dm->sf->index(row, G::YearColumn).data().toString();
        if (!yearList.contains(year)) yearList.append(year);
        QString day = dm->sf->index(row, G::DayColumn).data().toString();
        if (!dayList.contains(day)) dayList.append(day);
        QStringList itemKeywordList = dm->sf->index(row, G::KeywordsColumn).data().toStringList();
        for (int i = 0; i < itemKeywordList.size(); i++) {
            QString keyWord = itemKeywordList.at(i);
            if (!keywordList.contains(keyWord)) keywordList.append(keyWord);
        }
    }
//    qDebug() << "BuildFilters::mapUniqueInstances" << creatorList;
    // populate count map for progress
    totUniqueItems = 0;      // total fixed instances ie search, rating, labels etc
    int x;

//    x = refineList.count();
//    totInstances += x;
//    instances[" Refine"] = x;

    x = pickList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catPick] = x;

    x = ratingList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catRating] = x;
//    qDebug() << "BuildFilters::mapUniqueInstances  ratingList =" << ratingList;

    x = labelList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catLabel] = x;

    x = typeList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catType] = x;

    x = modelList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catModel] = x;

    x = lensList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catLens] = x;

    x = titleList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catTitle] = x;

    x = flList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catFocalLength] = x;

    x = creatorList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catCreator] = x;

    x = yearList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catYear] = x;

    totUniqueItems += x;
    uniqueItemCount[filters->catDay] = x;
    x = keywordList.count();
    totUniqueItems += x;
    uniqueItemCount[filters->catKeyword] = x;

    qDebug() << "BuildFilters::mapUniqueInstances instances =" << uniqueItemCount;

    // build filter item maps
//    filters->addCategoryFromData(refineList, filters->refine);
    filters->addCategoryFromData(pickList, filters->picks);
    filters->addCategoryFromData(ratingList, filters->ratings);
    filters->addCategoryFromData(labelList, filters->labels);
    filters->addCategoryFromData(typeList, filters->types);
    filters->addCategoryFromData(modelList, filters->models);
    filters->addCategoryFromData(lensList, filters->lenses);
    filters->addCategoryFromData(flList, filters->focalLengths);
    filters->addCategoryFromData(yearList, filters->years);
    filters->addCategoryFromData(dayList, filters->days);
    filters->addCategoryFromData(titleList, filters->titles);
    filters->addCategoryFromData(keywordList, filters->keywords);
    filters->addCategoryFromData(creatorList, filters->creators);
}

void BuildFilters::run()
{
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log("BuildFilters::run"); mutex.unlock();}

    if (filters->filtersBuilt) return;
    if (!abort) loadAllMetadata();
    if (!abort) mapUniqueInstances();
    if (!abort) countFiltered();
    if (!abort) countUnfiltered();
    filters->disableEmptyCat();
    done();
}
