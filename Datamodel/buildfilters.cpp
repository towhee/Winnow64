#include "buildfilters.h"

BuildFilters::BuildFilters(QObject *parent,
                           DataModel *dm,
                           Metadata *metadata,
                           Filters *filters,
                           bool &combineRawJpg) :
                           QThread(parent),
                           combineRawJpg(combineRawJpg)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
        filters->disableZeroCountItems(true);
        filters->setEnabled(true);
        filters->collapseAll();
//        emit updateIsRunning(false);
    }
    if (G::stop) emit stopped("BuildFilters");
}

void BuildFilters::build()
{
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    instance = dm->instance;
    filters->startBuildFilters();
    progress = 0;
    dmRows = dm->rowCount();
    buildFiltersTimer.restart();
    start(NormalPriority);
}

void BuildFilters::done()
{
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
    if (!abort) emit finishedBuildFilters();
//    qint64 msec = buildFiltersTimer.elapsed();
//    qDebug() << "BuildFilters::done" << QString("%L1").arg(msec) << "msec";
}

void BuildFilters::unfilteredItemSearchCount()
{
/*
    This function counts the number of occurences of each unique item in the datamodel, and
    also if raw+jpg have been combined. The results as saved in the filters QTreeWidget in
    columns 3 (raw+jpg) and 4 (all).

    This function is run everytime the search string changes.
*/
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log(CLASSFUNCTION); mutex.unlock();}
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
    filters->disableZeroCountItems(true);
    filters->filtersBuilt = true;
    filters->buildingFilters = false;
    filters->update();
}

void BuildFilters::countFiltered()
{
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log(CLASSFUNCTION); mutex.unlock();}
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
                if (cat == " Keywords") {
                    QStringList x = dm->index(row, col).data().toStringList();
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
        }
        if (!(*it)->parent()) {
            if (instances.contains(cat)) {
                int itemProgress = 40 * instances[cat] / totInstances;
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
    filters->disableZeroCountItems(true);
    progress = 60;
}

void BuildFilters::countUnfiltered()
{
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log(CLASSFUNCTION); mutex.unlock();}
    // count unfiltered
    QTreeWidgetItemIterator it(filters);
    QString cat = "";    // category ie Search, Ratings, Labels, etc
    while (*it) {
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
                if (cat == " Keywords") {
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
        if (!(*it)->parent()) {
            if (instances.contains(cat)) {
                int itemProgress = 40 * instances[cat] / totInstances;
                progress += itemProgress;
                emit updateProgress(progress);
                /*
                qDebug() << "BuildFilters::countUnfiltered"
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
    progress = 100;
}

void BuildFilters::loadAllMetadata()
{
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
                dm->addMetadataForItem(metadata->m, "BuildFilters::loadAllMetadata");
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
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log(CLASSFUNCTION); mutex.unlock();}
    // collect all unique instances for filtration (use QMap to maintain order)
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
    // populate count map for progress
    totInstances = 0;      // total fixed instances ie search, rating, labels etc
    int x = typeList.count();
    totInstances += x;
    instances[" File type"] = x;
    x = modelList.count();
    totInstances += x;
    instances[" Camera model"] = x;
    x = lensList.count();
    totInstances += x;
    instances[" Lenses"] = x;
    x = titleList.count();
    totInstances += x;
    instances[" Title"] = x;
    x = flList.count();
    totInstances += x;
    instances[" FocalLengths"] = x;
    x = creatorList.count();
    totInstances += x;
    instances[" Creators"] = x;
    x = yearList.count();
    totInstances += x;
    instances[" Years"] = x;
    totInstances += x;
    instances[" Days"] = x;
    x = keywordList.count();
    totInstances += x;
    instances[" Keywords"] = x;

    // build filter item maps
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
    if (G::isLogger || G::isFlowLogger) {mutex.lock(); G::log(CLASSFUNCTION); mutex.unlock();}
    if (filters->filtersBuilt) return;
    if (!abort) loadAllMetadata();
    if (!abort) mapUniqueInstances();
    if (!abort) countFiltered();
    if (!abort) countUnfiltered();
    done();
}


















