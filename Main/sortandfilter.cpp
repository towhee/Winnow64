#include "Main/mainwindow.h"

/*  *******************************************************************************************

    FILTERS & SORTING

    Filters have two types:

        1. Metadata known by the operating system (name, suffix, size, modified date)
        2. Metadata read from the file by Winnow (ie cameera model, aperture, ...)

    When a folder is first selected only type 1 information is known for all the files in the
    folder unless the folder is small enough so all the files were read in one pass. Filtering
    and sorting can only occur when the filter item is known for all the files. This is
    tracked by G::metaReadDone.

    Type 2 Filtration steps:

        * Make sure all metadata has been loaded for all files
        * Build the filters
        * Filter based on criteria selected
*/

void MW::filterDockTabMousePress()
{
    if (G::isLogger) G::log("MW::filterDockVisibilityChange");
    // qDebug() << "MW::filterDockTabMousePress" << "filterDock->isVisible() =" << filterDock->isVisible();

    // Clicking on the filter dock tab toggles visibility before this function is called,
    // so test for the opposite.
    if (filterDock->isVisible() && !filters->filtersBuilt) {
        buildFilters->build();
    }
}

void MW::updateAllFilters()
{
    if (G::isLogger) G::log("MW::updateAllFilters");
    qDebug() << "MW::updateAllFilters buildFilters->build(BuildFilters::Update)";
    buildFilters->build();
//    buildFilters->build(BuildFilters::Update);
}

void MW::launchBuildFilters(bool force)
{
/*
    Filters cannot be updated if hidden.
    Check if can build: filterDock->visibleRegion().isNull()
    If images have been deleted then force a rebuild
*/
    if (G::isLogger) G::log("MW::launchBuildFilters");
    if (G::isInitializing) return;
    if (!G::metaReadDone) {
        G::popUp->showPopup("Not all data required for filtering has been loaded yet.", 2000);
        return;
    }
    if (filters->filtersBuilt && !force) return;

    //qDebug() << "MW::launchBuildFilters buildFilters->build()";
    buildFilters->build();
}

void MW::filterChange(QString source)
{
/*
    All filter changes should be routed to here as a central clearing house.

    - the datamodel instance is incremented
    - the datamodel proxy filter is refreshed
    - the filter panel counts are updated
    - the current index is updated
    - any prior selection that is still available is set
    - the image cache is rebuilt to match the current filter
    - the thumb and grid first/last/thumbsPerPage parameters are recalculated
      and icons are loaded if necessary.
*/
    if (G::isLogger || G::isFlowLogger) qDebug() << "MW::filterChange  Src: " << source;
    qDebug() << "MW::filterChange" << "called from:" << source;

    // ignore if new folder is being loaded
    if (!G::metaReadDone) {
        G::popUp->showPopup("Please wait for the folder to complete loading...", 2000);
        return;
    }
    if (G::stop) return;

    //G::popUp->showPopup("Executing filter...", 0);
    // increment the dm->instance.  This is necessary to ignore any updates to ImageCache
    // and MetaRead2 for the prior datamodel filter.
    dm->newInstance();
    // stop ImageCache
    imageCacheThread->stop();

    // if filter change source is the filter panel then sync menu actions isChecked property
    if (source == "Filters::itemClickedSignal") filterSyncActionsWithFilters();
    /*
    // Need all metadata loaded before filtering
    if (source != "MW::clearAllFilters") {
        dm->forceBuildFilters = true;
        if (!G::metaReadDone) dm->addAllMetadata();
        // failed to load all metadata - maybe terminated by user pressing ESC

        if (!G::metaReadDone) {
            G::popUp->showPopup("Failed to load all metadata...");
            return;
        }
    } //*/
    QApplication::setOverrideCursor(Qt::WaitCursor);

     // prevent unwanted fileSelectionChange()
    isFilterChange = true;

    // refresh the proxy sort/filter, which updates the selectionIndex, which triggers a
    // scroll event and the metadataCache updates the icons and thumbnails
    dm->sf->suspend(false);
    dm->sf->filterChange("MW::filterChange");

    // update filter panel image count by filter item
    buildFilters->update();

    // recover sort after filtration
    //sortChange("filterChange");

    // allow fileSelectionChange()
    isFilterChange = false;

    // update the status panel filtration status
    updateStatusBar();

    // if filter has eliminated all rows so nothing to show
    if (!dm->sf->rowCount()) {
        nullFiltration();
        QApplication::restoreOverrideCursor();
        G::popUp->reset();
        return;
    }

    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    // is the DataModel current index still in the filter.  If not, reset
    QModelIndex newSfIdx = dm->sf->mapFromSource(dm->currentDmIdx);
    bool newSelectReqd = false;
    int oldRow = dm->currentDmIdx.row();
    int sfRows = dm->sf->rowCount();
    if (!newSfIdx.isValid()) {
        newSelectReqd = true;
        if (oldRow < sfRows) newSfIdx = dm->sf->index(oldRow, 0);
        else newSfIdx = dm->sf->index(sfRows-1, 0);
    }

    // rebuild imageCacheList and update priorities in image cache
    QString fPath = newSfIdx.data(G::PathRole).toString();
    imageCacheThread->rebuildImageCacheParameters(fPath, "FilterChange");

    // select after filtration
    if (newSelectReqd) {
        sel->select(newSfIdx);
    }
    else {
        sel->updateCurrentIndex(newSfIdx);
    }
    thumbView->scrollToCurrent("MW::filterChange");

    QApplication::restoreOverrideCursor();
    G::popUp->reset();
}

void MW::quickFilter()
{
    if (G::isLogger) G::log("MW::quickFilter");

    // make sure the filters have been built
    //qDebug() << "MW::quickFilter  filters->filtersBuilt =" << filters->filtersBuilt;
    if (filters->filtersBuilt) {
        quickFilterComplete();
    }
    else {
        filterDock->setVisible(true);       // triggers launchBuildFilters()
        filterDock->raise();
        qDebug() << "MW::quickFilter buildFilters->build(BuildFilters::Reset, 'QuickFilter')";
        filterDockVisibleAction->setChecked(true);
        buildFilters->build(BuildFilters::QuickFilter);
    }
}

void MW::quickFilterComplete()
{
    if (G::isLogger) G::log("MW::quickFilter");
    filters->setRatingState("1", filterRating1Action->isChecked());
    filters->setRatingState("2", filterRating2Action->isChecked());
    filters->setRatingState("3", filterRating3Action->isChecked());
    filters->setRatingState("4", filterRating4Action->isChecked());
    filters->setRatingState("5", filterRating5Action->isChecked());
    filters->setLabelState("Red", filterRedAction->isChecked());
    filters->setLabelState("Yellow", filterYellowAction->isChecked());
    filters->setLabelState("Green", filterGreenAction->isChecked());
    filters->setLabelState("Blue", filterBlueAction->isChecked());
    filters->setLabelState("Purple", filterPurpleAction->isChecked());

    filters->setEachCatTextColor();
    filterChange("MW::quickFilter");
}

void MW::filterSyncActionsWithFilters()
{
    if (G::isLogger) G::log("MW::filterSyncActionsWithFilters");
    filterRating1Action->setChecked(filters->isRatingChecked("1"));
    filterRating2Action->setChecked(filters->isRatingChecked("2"));
    filterRating3Action->setChecked(filters->isRatingChecked("3"));
    filterRating4Action->setChecked(filters->isRatingChecked("4"));
    filterRating5Action->setChecked(filters->isRatingChecked("5"));
    filterRedAction->setChecked(filters->isLabelChecked("Red"));
    filterYellowAction->setChecked(filters->isLabelChecked("Yellow"));
    filterGreenAction->setChecked(filters->isLabelChecked("Green"));
    filterBlueAction->setChecked(filters->isLabelChecked("Blue"));
    filterPurpleAction->setChecked(filters->isLabelChecked("Purple"));
    filterLastDayAction->setChecked(filters->isOnlyMostRecentDayChecked());
}

void MW::invertFilters()
{
/*

*/
    if (G::isLogger) G::log("MW::invertFilters");
    if (!G::metaReadDone) loadEntireMetadataCache("FilterChange");

    if (dm->rowCount() == 0) {
        G::popUp->showPopup("No images available to invert filtration", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }
    if (dm->rowCount() == dm->sf->rowCount()) {
        G::popUp->showPopup("No filters assigned - null inversion would result", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }
    filters->invertFilters();

    filterChange("MW::invertFilters");
}

void MW::uncheckAllFilters()
{
    if (G::isLogger) G::log("MW::uncheckAllFilters");
    filters->uncheckAllFilters();
    filterPickAction->setChecked(false);
    filterLastDayAction->setChecked(false);
    /*
    filterRating1Action->setChecked(false);
    filterRating2Action->setChecked(false);
    filterRating3Action->setChecked(false);
    filterRating4Action->setChecked(false);
    filterRating5Action->setChecked(false);
    filterRedAction->setChecked(false);
    filterYellowAction->setChecked(false);
    filterGreenAction->setChecked(false);
    filterBlueAction->setChecked(false);
    filterPurpleAction->setChecked(false);
    */
}

void MW::clearAllFilters()
{
    if (G::isLogger) G::log("MW::clearAllFilters");
    if (!G::metaReadDone) loadEntireMetadataCache("FilterChange");   // rgh is this reqd
    uncheckAllFilters();
    filters->searchString = "";
    dm->searchStringChange("");
    filterChange("MW::clearAllFilters");
}

void MW::setFilterSolo()
{
    if (G::isLogger) G::log("MW::setFilterSolo");
    filters->setSoloMode(filterSoloAction->isChecked());
    settings->setValue("isSoloFilters", filterSoloAction->isChecked());
}

void MW::filterLastDay()
{
/*
.
*/
    if (G::isLogger) G::log("MW::filterLastDay");
    if (dm->sf->rowCount() == 0 && filterLastDayAction->isChecked()) {
        G::popUp->showPopup("No images available to filter", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }

    // if the additional filters have not been built then do an update
    if (!filters->filtersBuilt) {
//        qDebug() << "MW::filterLastDay" << "build filters";
//        launchBuildFilters();
//        G::popUp->showPopup("Building filters.", 0);
//        buildFilters->wait();
//        G::popUp->end();
        filterDock->setVisible(true);       // triggers launchBuildFilters()
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        qDebug() << "MW::filterLasstDay buildFilters->build(BuildFilters::Reset, 'FilterLastDay')";
        buildFilters->build(BuildFilters::MostRecentDay);
        return;
//    if (!filters->days->childCount()) launchBuildFilters();
    }

    // if there still are no days then tell user and return
    int last = filters->days->childCount();
    if (last == 0) {
        G::popUp->showPopup("No days are available to filter", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }

    if (filterLastDayAction->isChecked()) {
        filters->days->child(last - 1)->setCheckState(0, Qt::Checked);
    }
    else {
        filters->days->child(last - 1)->setCheckState(0, Qt::Unchecked);
    }

    filterChange("MW::filterLastDay");
}

void MW::sortChangeFromAction()
{
/*
    When a sort change menu item is picked this function is called, and then redirects to
    sortChange.
*/
    if (G::isLogger) G::log("MW::sortChangeFromAction");

    if (sortFileNameAction->isChecked()) sortColumn = G::NameColumn;        // core
    if (sortFileTypeAction->isChecked()) sortColumn = G::TypeColumn;        // core
    if (sortFileSizeAction->isChecked()) sortColumn = G::SizeColumn;        // core
    if (sortCreateAction->isChecked()) sortColumn = G::CreatedColumn;       // core
    if (sortModifyAction->isChecked()) sortColumn = G::ModifiedColumn;      // core
    if (sortPickAction->isChecked()) sortColumn = G::PickColumn;            // core
    if (sortLabelAction->isChecked()) sortColumn = G::LabelColumn;          // core
    if (sortRatingAction->isChecked()) sortColumn = G::RatingColumn;        // core
    if (sortMegaPixelsAction->isChecked()) sortColumn = G::MegaPixelsColumn;
    if (sortDimensionsAction->isChecked()) sortColumn = G::DimensionsColumn;
    if (sortApertureAction->isChecked()) sortColumn = G::ApertureColumn;
    if (sortShutterSpeedAction->isChecked()) sortColumn = G::ShutterspeedColumn;
    if (sortISOAction->isChecked()) sortColumn = G::ISOColumn;
    if (sortModelAction->isChecked()) sortColumn = G::CameraModelColumn;
    if (sortLensAction->isChecked()) sortColumn = G::LensColumn;
    if (sortFocalLengthAction->isChecked()) sortColumn = G::FocalLengthColumn;
    if (sortTitleAction->isChecked()) sortColumn = G::TitleColumn;
    if (sortCreatorAction->isChecked()) sortColumn = G::CreatorColumn;
    sortChange("Action");
}

void MW::sortChange(QString source)
{
/*
    Triggered by a menu sort item, a new folder or a new workspace. Core sort items (QFileInfo
    items) are always loaded into the datamodel, so we can sort on them at any time. Non-core
    items, read from the image file metadata, are only loaded on demand. All non-core items
    must be loaded in order to sort on them.

    The sort order (ascending or descending) can be set by the menu, the button icon on the
    statusbar or a workspace change.
*/
    if (G::isLogger || G::isFlowLogger) qDebug() << "MW::sortChange  Src:" << source;
    //qDebug() << "MW::sortChange  Src:" << source;

    if (G::isInitializing || !G::metaReadDone) return;

    QList<G::dataModelColumns> coreSorts;
    coreSorts << G::NameColumn << G::TypeColumn << G::SizeColumn << G::CreatedColumn << G::ModifiedColumn;

    /*
    qDebug() << "MW::sortChange" << "source =" << source
             << "G::isLinearLoadDone =" << G::isLinearLoadDone
             << "G::isInitializing =" << G::isInitializing
             << "prevSortColumn =" << prevSortColumn
             << "sortColumn =" << sortColumn
             << "isReverseSort =" << isReverseSort
             << "prevIsReverseSort =" << prevIsReverseSort
             << "coreSorts =" << coreSorts
                ;
//                */

    // reset sort to file name if was sorting on non-core metadata while folder still loading
    if (!G::metaReadDone && !coreSorts.contains(sortColumn)) {
        prevSortColumn = G::NameColumn;
        updateSortColumn(G::NameColumn);
    }

    // check if sorting has changed
    bool sortHasChanged = false;
    if (sortColumn != prevSortColumn || isReverseSort != prevIsReverseSort) {
        sortHasChanged = true;
        prevSortColumn = sortColumn;
        prevIsReverseSort = isReverseSort;
    }

    // do not sort conditions
    /*
    qDebug() << "MW::sortChange"
             << "sortMenuUpdateToMatchTable =" << sortMenuUpdateToMatchTable
             << "G::metaReadDone =" << G::metaReadDone
             << "G::allMetadataLoaded =" << G::allMetadataLoaded
             << "sortColumn =" << sortColumn
             << "G::NameColumn =" << G::NameColumn
             << "sortHasChanged =" << sortHasChanged
        ;
//*/
    bool doNotSort = false;
    if (sortMenuUpdateToMatchTable)
        doNotSort = true;
    if (!G::metaReadDone && sortColumn > G::CreatedColumn)
        doNotSort = true;
    if (!G::metaReadDone && sortColumn == G::NameColumn && !sortReverseAction->isChecked())
        doNotSort = true;
    if (G::metaReadDone && !sortHasChanged)
        doNotSort = true;
    if (doNotSort) return;

    // Need all metadata loaded before sorting non-fileSystem metadata
    // rgh all metadata always loaded now - change this?
    if (!G::metaReadDone && sortColumn > G::CreatedColumn)
        loadEntireMetadataCache("SortChange");

    // failed to load all metadata, restore prior sort in menu and return
    if (!G::metaReadDone && sortColumn > G::CreatedColumn) {
        /*
        qDebug() << "MW::sortChange" << "failed"
                 << "sortColumn =" << sortColumn
                 << "prevSortColumn =" << prevSortColumn
                 ;
//                 */
        updateSortColumn(prevSortColumn);
        return;
    }

    prevSortColumn = sortColumn;
    /*
    qDebug() << "MW::sortChange" << "succeeded"
             << "sortColumn =" << sortColumn
             << "prevSortColumn =" << prevSortColumn
             << "  Commencing sort"
             ;
//             */

    if (G::metaReadDone) {
        G::popUp->showPopup("Sorting...", 0);
    }
    else {
        setCentralMessage("Sorting images");
    }

    // save selection prior to sorting
    sel->save();
    thumbView->sortThumbs(sortColumn, isReverseSort);
    sel->recover();

//    if (!G::metaReadDone) return;

    // get the current selected item
    if (G::metaReadDone) dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    else dm->currentSfRow = 0;

    thumbView->iconViewDelegate->currentRow = dm->currentSfRow;
    gridView->iconViewDelegate->currentRow = dm->currentSfRow;
    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    //dm->selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(dm->currentSfRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache and EmbelProperties
    //dm->currentFilePath = fPath;

    centralLayout->setCurrentIndex(prevCentralView);
    updateStatus(true, "", "MW::sortChange");

    // sync image cache with datamodel filtered proxy unless sort has been triggered by a
    // filter change, which will do its own rebuildImageCacheParameters
    if (source != "filterChange")
        imageCacheThread->rebuildImageCacheParameters(fPath, "SortChange");

    /* if the previous selected image is also part of the filtered datamodel then the
       selected index does not change and fileSelectionChange will not be signalled.
       Therefore we call it here to force the update to caching and icons */
//    sel->select(idx);

    scrollToCurrentRow();
    G::popUp->reset();

}

void MW::sortReverse()
/*
    Experiment to reverse sort on the file name while a new folder is loading.
*/
{
    thumbView->sortThumbs(G::NameColumn, isReverseSort);

    //    if (!G::metaReadDone) return;

    // get the current selected item
    if (G::metaReadDone) dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    else dm->currentSfRow = 0;

    thumbView->iconViewDelegate->currentRow = dm->currentSfRow;
    gridView->iconViewDelegate->currentRow = dm->currentSfRow;
    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    dm->selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(dm->currentSfRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache and EmbelProperties
    dm->currentFilePath = fPath;
    sel->select(idx);

    scrollToCurrentRow();
}

void MW::updateSortColumn(int sortColumn)
{
    if (G::isLogger) G::log("MW::updateSortColumn");

    if (sortColumn == 0) sortColumn = G::NameColumn;

    if (sortColumn == G::NameColumn) sortFileNameAction->setChecked(true);
    if (sortColumn == G::TypeColumn) sortFileTypeAction->setChecked(true);
    if (sortColumn == G::SizeColumn) sortFileSizeAction->setChecked(true);
    if (sortColumn == G::CreatedColumn) sortCreateAction->setChecked(true);
    if (sortColumn == G::ModifiedColumn) sortModifyAction->setChecked(true);
    if (sortColumn == G::PickColumn) sortPickAction->setChecked(true);
    if (sortColumn == G::LabelColumn) sortLabelAction->setChecked(true);
    if (sortColumn == G::RatingColumn) sortRatingAction->setChecked(true);
    if (sortColumn == G::MegaPixelsColumn) sortMegaPixelsAction->setChecked(true);
    if (sortColumn == G::DimensionsColumn) sortDimensionsAction->setChecked(true);
    if (sortColumn == G::ApertureColumn) sortApertureAction->setChecked(true);
    if (sortColumn == G::ShutterspeedColumn) sortShutterSpeedAction->setChecked(true);
    if (sortColumn == G::ISOColumn) sortISOAction->setChecked(true);
    if (sortColumn == G::CameraModelColumn) sortModelAction->setChecked(true);
    if (sortColumn == G::FocalLengthColumn) sortFocalLengthAction->setChecked(true);
    if (sortColumn == G::TitleColumn) sortTitleAction->setChecked(true);
}

void MW::toggleSortDirectionClick()
{
/*
    This is called by connect signals from the menu action and the reverse sort button.  The
    call is redirected to toggleSortDirection, which has a parameter which is not supported
    by the action and button signals.
*/
    if (G::isLogger) G::log("MW::toggleSortDirectionClick");
    toggleSortDirection(Tog::toggle);
    sortChange("MW::toggleSortDirectionClick");
    // Experiment to reverse sort on the file name while a new folder is loading.
    // sortReverse();
}

void MW::toggleSortDirection(Tog n)
{
    if (G::isLogger) G::log("MW::toggleSortDirection");
    if (prevIsReverseSort == isReverseSort)
    prevIsReverseSort = isReverseSort;
    if (n == Tog::toggle) isReverseSort = !isReverseSort;
    if (n == Tog::off) isReverseSort = false;
    if (n == Tog::on) isReverseSort = true;
    if (isReverseSort) {
        sortReverseAction->setChecked(true);
        reverseSortBtn->setIcon(QIcon(":/images/icon16/Z-A.png"));
    }
    else {
        sortReverseAction->setChecked(false);
        reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));
    }
}

void MW::setRating()
{
/*
    Resolve the source menu action that called (could be any rating) and then set the rating
    for all the selected thumbs.
*/
    if (G::isLogger) G::log("MW::setRating");
    qDebug() << "MW::setRating";
    // do not set rating if slideshow is on
    if (G::isSlideShow) return;

    // make sure classification badges are visible
    if (!isRatingBadgeVisible) {
        ratingBadgeVisibleAction->setChecked(true);
        isRatingBadgeVisible = true;
        thumbView->refreshThumbs();
        gridView->refreshThumbs();
    }

    QObject* obj = sender();
    QString s = obj->objectName();
    if (s == "Rate0") rating = "";
    if (s == "Rate1") rating = "1";
    if (s == "Rate2") rating = "2";
    if (s == "Rate3") rating = "3";
    if (s == "Rate4") rating = "4";
    if (s == "Rate5") rating = "5";

    QModelIndexList selection = dm->selectionModel->selectedRows();
    // check if selection is entirely rating already - if so set no rating
    bool isAlreadyRating = true;
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->sf->index(selection.at(i).row(), G::RatingColumn);
        bool isDifferent = idx.data(Qt::EditRole).toString() != rating;
        /*
        qDebug() << "MW::setRating isAlreadyRating"
                 << "selection iter =" << i
                 << "row = " << selection.at(i).row()
                 << "value =" << idx.data(Qt::EditRole)
                 << "isDiffernt =" << isDifferent
                    ; //*/
        if (idx.data(Qt::EditRole).toString() != rating) {
            isAlreadyRating = false;
            break;
        }
    }
    if (isAlreadyRating) rating = "";     // invert the rating(s)

    int n = selection.count();
    /*
    if (G::useSidecar) {
        G::popUp->setProgressVisible(true);
        G::popUp->setProgressMax(n + 1);
        QString txt = "Writing to XMP sidecar for " + QString::number(n) + " images." +
                      "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
        G::popUp->showPopup(txt, 0, true, 1);
    }
    //*/

    // copy selection to list of dm rows (proxy filter changes during iteration when change datamodel)
    QList<int> rows;
    for (int i = 0; i < n; ++i) {
        int dmRow = dm->modelRowFromProxyRow(selection.at(i).row());
        rows.append(dmRow);
    }

    // set the rating in the datamodel
    QString src = "MW::setRating";
    for (int i = 0; i < n; ++i) {
        int dmRow = rows.at(i);
        QModelIndex ratingIdx = dm->index(dmRow, G::RatingColumn);
        emit setValue(ratingIdx, rating, dm->instance, src, Qt::EditRole, Qt::AlignCenter);
        // update rating crash log
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::PathRole).toString();
        updateRatingLog(fPath, rating);
        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->index(dmRow, 0);
            // is this part of a raw+jpg pair
            if(idx.data(G::DupIsJpgRole).toBool()) {
                // set rating for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupOtherIdxRole));
                int rowDup = rawIdx.row();
                ratingIdx = dm->index(rowDup, G::RatingColumn);
                emit setValue(ratingIdx, rating, dm->instance, src, Qt::EditRole);
                // update rating crash log
                QString jpgPath  = dm->index(rowDup, G::PathColumn).data(G::PathRole).toString();
                updateRatingLog(jpgPath, rating);
            }
        }
        // write to sidecar
        if (G::useSidecar) {
            dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
            metadata->writeXMP(metadata->sidecarPath(fPath), "MW::setRating");
            // update _Rating (used to check what metadata has changed in metadata->writeXMP)
            QModelIndex _ratingIdx = dm->index(dmRow, G::_RatingColumn);
            emit setValue(_ratingIdx, rating, dm->instance, src, Qt::EditRole);
            G::popUp->setProgress(i+1);
        }
    }

    filterChange("MW::setRating");
    //dm->sf->suspend(true);
    buildFilters->updateCategory(BuildFilters::RatingEdit);
    //dm->sf->suspend(false);

    // update ImageView classification badge
    updateClassification();

    // update filter list and counts

    if (G::useSidecar) {
        G::popUp->setProgressVisible(false);
        G::popUp->reset();
    }

    // auto advance
    if (autoAdvance) sel->next();
}

int MW::ratingLogCount()
{
    if (G::isLogger) G::log("MW::ratingLogCount");
    settings->beginGroup("RatingLog");
    int count = settings->allKeys().size();
    settings->endGroup();
    return count;
}

void MW::recoverRatingLog()
{
    if (G::isLogger) G::log("MW::recoverRatingLog");
    settings->beginGroup("RatingLog");
    QStringList keys = settings->allKeys();
    QString src = "MW::recoverRatingLog";
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
                            QString pickStatus = settings->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        if (idx.isValid()) {
            QModelIndex ratingIdx = dm->sf->index(idx.row(), G::RatingColumn);
            emit setValueSf(ratingIdx, pickStatus, dm->instance, src, Qt::EditRole);
        }
        else {
//            qDebug() << "MW::recoverRatingLog" << fPath << "not found";
        }
    }
    settings->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearRatingLog()
{
    if (G::isLogger) G::log("MW::clearRatingLog");
    settings->beginGroup("RatingLog");
    QStringList keys = settings->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        settings->remove(keys.at(i));
    }
    settings->endGroup();
}

void MW::updateRatingLog(QString fPath, QString rating)
{
    if (G::isLogger) G::log("MW::updateRatingLog");
    settings->beginGroup("RatingLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (rating == "") {
        /*
        qDebug() << "MW::updateRatingLog" << "removing" << sKey;
        //*/
        settings->remove(sKey);
    }
    else {
        /*
        qDebug() << "MW::updateRatingLog" << "adding" << sKey;
        //*/
        settings->setValue(sKey, rating);
    }
    settings->endGroup();
}

void MW::setColorClass()
{
/*
    Resolve the source menu action that called (could be any color class) and then set the
    color class for all the selected thumbs.
*/
    if (G::isLogger) G::log("MW::setColorClass");
    //qDebug() << "MW::setColorClass";
    // do not set color class if slideshow is on
    if (G::isSlideShow) return;

    // make sure classification badges are visible
    if (!isRatingBadgeVisible) {
        ratingBadgeVisibleAction->setChecked(true);
        isRatingBadgeVisible = true;
        thumbView->refreshThumbs();
        gridView->refreshThumbs();
    }

    QObject* obj = sender();
    QString s = obj->objectName();
    if (s == "Label0") colorClass = "";
    if (s == "Label1") colorClass = "Red";
    if (s == "Label2") colorClass = "Yellow";
    if (s == "Label3") colorClass = "Green";
    if (s == "Label4") colorClass = "Blue";
    if (s == "Label5") colorClass = "Purple";

    QModelIndexList selection = dm->selectionModel->selectedRows();
    // check if selection is entirely label color already - if so set no label
    bool isAlreadyLabel = true;
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->sf->index(selection.at(i).row(), G::LabelColumn);
        if (idx.data(Qt::EditRole).toString() != colorClass) {
            isAlreadyLabel = false;
            break;
        }
    }
    if (isAlreadyLabel) colorClass = "";     // invert the label

    int n = selection.count();
    /*
    if (G::useSidecar) {
        G::popUp->setProgressVisible(true);
        G::popUp->setProgressMax(n + 1);
        QString txt = "Writing to XMP sidecar for " + QString::number(n) + " images." +
                      "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
        G::popUp->showPopup(txt, 0, true, 1);
    }
    //*/

    // copy selection to list of dm rows (proxy filter changes during iteration when
    // change datamodel)
    QList<int> rows;
    for (int i = 0; i < n; ++i) {
        int dmRow = dm->modelRowFromProxyRow(selection.at(i).row());
        rows.append(dmRow);
    }

    // update the data model
    QString src = "MW::setColorClass";
    for (int i = 0; i < n; ++i) {
        int dmRow = rows.at(i);
        QModelIndex labelIdx = dm->index(dmRow, G::LabelColumn);
        emit setValue(labelIdx, colorClass, dm->instance, src, Qt::EditRole, Qt::AlignCenter);
        // update color class crash log
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::PathRole).toString();
        updateColorClassLog(fPath, colorClass);
        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->index(dmRow, 0);
            // is this part of a raw+jpg pair
            if (idx.data(G::DupIsJpgRole).toBool()) {
                // set color class (label) for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupOtherIdxRole));
                int rowDup = rawIdx.row();
                labelIdx = dm->index(rowDup, G::LabelColumn);
                QString src = "MW::setColorClass";
                emit setValue(labelIdx, colorClass, dm->instance, src, Qt::EditRole, Qt::AlignCenter);
                // update color class crash log
                QString jpgPath = dm->sf->index(rowDup, G::PathColumn).data(G::PathRole).toString();
                updateColorClassLog(jpgPath, colorClass);
            }
        }
        // write to sidecar
        if (G::useSidecar) {
            dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
            metadata->writeXMP(metadata->sidecarPath(fPath), "MW::setColorClass");
            // update _Label (used to check what metadata has changed in metadata->writeXMP)
            QModelIndex _labelIdx = dm->index(dmRow, G::_LabelColumn);
            emit setValue(_labelIdx, colorClass, dm->instance, src, Qt::EditRole);
            G::popUp->setProgress(i+1);
        }
    }

    filterChange("MW::setColorClass");
    //dm->sf->suspend(true);
    buildFilters->updateCategory(BuildFilters::LabelEdit);
    //dm->sf->suspend(false);

    // update ImageView classification badge
    updateClassification();

    if (G::useSidecar) {
        G::popUp->setProgressVisible(false);
        G::popUp->reset();
    }

    // auto advance
    if (autoAdvance) sel->next();
}

int MW::colorClassLogCount()
{
    if (G::isLogger) G::log("MW::colorClassLogCount");
    settings->beginGroup("ColorClassLog");
    int count = settings->allKeys().size();
    settings->endGroup();
    return count;
}

void MW::recoverColorClassLog()
{
    if (G::isLogger) G::log("MW::recoverColorClassLog");
    settings->beginGroup("ColorClassLog");
    QStringList keys = settings->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
                            QString colorClassStatus = settings->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        QString src = "MW::recoverColorClassLog";
        if (idx.isValid()) {
            QModelIndex colorClassIdx = dm->sf->index(idx.row(), G::LabelColumn);
            emit setValueSf(colorClassIdx, colorClassStatus, dm->instance, src, Qt::EditRole);
        }
        else {
//            qDebug() << "MW::recoverColorClassLog" << fPath << "not found";
        }
    }
    settings->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearColorClassLog()
{
    if (G::isLogger) G::log("MW::clearColorClassLog");
    settings->beginGroup("ColorClassLog");
    QStringList keys = settings->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        settings->remove(keys.at(i));
    }
    settings->endGroup();
}

void MW::updateColorClassLog(QString fPath, QString label)
{
    if (G::isLogger) G::log("MW::updateColorClassLog");
    settings->beginGroup("ColorClassLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (label == "") {
//        qDebug() << "MW::updateColorClassLog" << "removing" << sKey;
        settings->remove(sKey);
    }
    else {
//        qDebug() << "MW::updateColorClassLog" << "adding" << sKey;
        settings->setValue(sKey, label);
    }
    settings->endGroup();
}

void MW::searchTextEdit()
{
    if (G::isLogger) G::log("MW::searchTextEdit");

    if (!filters->filtersBuilt) {
        filterDock->setVisible(true);       // triggers launchBuildFilters()
        filterDock->raise();
        filterDockVisibleAction->setChecked(true);
        buildFilters->build(BuildFilters::Search);
    }

    // set visibility
    if (!filterDock->isVisible()) {
        filterDock->setVisible(true);
    }
    // set focus
    if (filterDock->visibleRegion().isEmpty()) {
        filterDock->raise();
    }
    // set menu status for filterDock in window menu
    filterDockVisibleAction->setChecked(true);

    // edit search text after this function returns
    QTimer::singleShot(100, this, SLOT(searchTextEdit2()));
    return;
}

void MW::searchTextEdit2()
{
    if (G::isLogger) G::log("MW::searchTextEdit2");
    // Goto item and edit
    filters->scrollToItem(filters->search);
    filters->expandItem(filters->search);
    filters->editItem(filters->searchTrue, 0);
    //buildFilters->update();
    return;
}

