#include "Main/mainwindow.h"

/*  *******************************************************************************************

    FILTERS & SORTING

    Filters have two types:

        1. Metadata known by the operating system (name, suffix, size, modified date)
        2. Metadata read from the file by Winnow (ie cameera model, aperture, ...)

    When a folder is first selected only type 1 information is known for all the files in the
    folder unless the folder is small enough so all the files were read in one pass. Filtering
    and sorting can only occur when the filter item is known for all the files. This is
    tracked by G::allMetadataLoaded.

    Type 2 Filtration steps:

        * Make sure all metadata has been loaded for all files
        * Build the filters
        * Filter based on criteria selected
*/

void MW::filterDockVisibilityChange(bool isVisible)
{
    if (G::isLogger) G::log(__FUNCTION__);
//    qDebug() << __FUNCTION__ << isVisible;
    if (isVisible && !G::isInitializing) launchBuildFilters();
}

void MW::launchBuildFilters()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::isInitializing) return;
    if (filterDock->visibleRegion().isNull()) {
        filters->setSoloMode(filterSoloAction->isChecked());
//        G::popUp->showPopup("Filters will only be updated when the filters panel is visible.");
//        return;
    }
    if (filters->filtersBuilt) {
        G::popUp->showPopup("Filters are up-to-date.");
        return;
    }
    if (dm->loadingModel) {
        G::popUp->showPopup("Not all data required for filtering has been loaded yet.", 2000);
        return;
    }

    filters->msgFrame->setVisible(true);
    buildFilters->build();
}

void MW::filterChange(QString source)
{
/*
    All filter changes should be routed to here as a central clearing house. The datamodel
    filter is refreshed, the filter panel counts are updated, the current index is updated,
    the image cache is rebuilt to match the current filter, any prior selection that is still
    available is set, the thumb and grid first/last/thumbsPerPage parameters are recalculated
    and icons are loaded if necessary.
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, "Src: " + source);
    qDebug() << __FUNCTION__ << "called from:" << source;
    // ignore if new folder is being loaded
    if (!G::isNewFolderLoaded) {
        G::popUp->showPopup("Please wait for the folder to complete loading...", 2000);
        return;
    }

    if (G::stop) return;

    // update filter checkbox
    qApp->processEvents();

    // if filter chnage source is the filter panel then sync menu actions isChecked property
    if (source == "Filters::itemClickedSignal") filterSyncActionsWithFilters();

//    imageCacheThread->pauseImageCache();

    // Need all metadata loaded before filtering
    if (source != "MW::clearAllFilters") {
        dm->forceBuildFilters = true;
        if (!G::allMetadataLoaded) dm->addAllMetadata();
        // failed to load all metadata - maybe terminated by user pressing ESC
        if (!G::allMetadataLoaded) return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // refresh the proxy sort/filter, which updates the selectionIndex, which triggers a
    // scroll event and the metadataCache updates the icons and thumbnails
    isFilterChange = true;      // prevent unwanted fileSelectionChange()
    dm->sf->filterChange();

    // update filter panel image count by filter item
    buildFilters->updateCountFiltered();
    if (source == "Filters::itemChangedSignal search text change") buildFilters->unfilteredItemSearchCount();

    // recover sort after filtration
    sortChange("filterChange");

    isFilterChange = false;     // allow fileSelectionChange()

    // update the status panel filtration status
    updateStatusBar();

    // if filter has eliminated all rows so nothing to show
    if (!dm->sf->rowCount()) {
        nullFiltration();
        QApplication::restoreOverrideCursor();
        return;
    }

    // get the current selected item
    currentRow = dm->sf->mapFromSource(currentDmIdx).row();
    // check if still in filtered set, if not select first item in filtered set
    if (currentRow == -1) currentRow = 0;
    thumbView->iconViewDelegate->currentRow = currentRow;
    gridView->iconViewDelegate->currentRow = currentRow;
    thumbView->selectThumb(currentRow);
    QModelIndex idx = dm->sf->index(currentRow, 0);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache
    dm->currentFilePath = fPath;

//    centralLayout->setCurrentIndex(prevCentralView);
    updateStatus(true, "", __FUNCTION__);

    // sync image cache with datamodel filtered proxy dm->sf
    imageCacheThread->rebuildImageCacheParameters(fPath, __FUNCTION__);
    /*
    if (thumbView->waitUntilOkToScroll()) {
        // setting the selection index also triggers fileSelectionChange()
        selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
        thumbView->selectThumb(idx);
    }
//    */
//    qDebug() << __FUNCTION__ << idx.data() << "Calling fileSelectionChange(idx, idx)";

    QApplication::restoreOverrideCursor();

    qDebug() << __FUNCTION__ << "Calling fileSelectionChange";
    fileSelectionChange(idx, idx, __FUNCTION__);
    source = "";    // suppress compiler warning
}

void MW::quickFilter()
{
    if (G::isLogger) G::log(__FUNCTION__);

    // make sure the filters have been built
    if (!filters->filtersBuilt) buildFilters->build();

    // checked
    if (filterSearchAction->isChecked()) filters->searchTrue->setCheckState(0, Qt::Checked);
    if (filterRating1Action->isChecked()) filters->ratings1->setCheckState(0, Qt::Checked);
    if (filterRating2Action->isChecked()) filters->ratings2->setCheckState(0, Qt::Checked);
    if (filterRating3Action->isChecked()) filters->ratings3->setCheckState(0, Qt::Checked);
    if (filterRating4Action->isChecked()) filters->ratings4->setCheckState(0, Qt::Checked);
    if (filterRating5Action->isChecked()) filters->ratings5->setCheckState(0, Qt::Checked);
    if (filterRedAction->isChecked()) filters->labelsRed->setCheckState(0, Qt::Checked);
    if (filterYellowAction->isChecked()) filters->labelsYellow->setCheckState(0, Qt::Checked);
    if (filterGreenAction->isChecked()) filters->labelsGreen->setCheckState(0, Qt::Checked);
    if (filterBlueAction->isChecked()) filters->labelsBlue->setCheckState(0, Qt::Checked);
    if (filterPurpleAction->isChecked()) filters->labelsPurple->setCheckState(0, Qt::Checked);

    // unchecked
    if (!filterRating1Action->isChecked()) filters->ratings1->setCheckState(0, Qt::Unchecked);
    if (!filterRating2Action->isChecked()) filters->ratings2->setCheckState(0, Qt::Unchecked);
    if (!filterRating3Action->isChecked()) filters->ratings3->setCheckState(0, Qt::Unchecked);
    if (!filterRating4Action->isChecked()) filters->ratings4->setCheckState(0, Qt::Unchecked);
    if (!filterRating5Action->isChecked()) filters->ratings5->setCheckState(0, Qt::Unchecked);
    if (!filterRedAction->isChecked()) filters->labelsRed->setCheckState(0, Qt::Unchecked);
    if (!filterYellowAction->isChecked()) filters->labelsYellow->setCheckState(0, Qt::Unchecked);
    if (!filterGreenAction->isChecked()) filters->labelsGreen->setCheckState(0, Qt::Unchecked);
    if (!filterBlueAction->isChecked()) filters->labelsBlue->setCheckState(0, Qt::Unchecked);
    if (!filterPurpleAction->isChecked()) filters->labelsPurple->setCheckState(0, Qt::Unchecked);

    filterChange("MW::quickFilter");
}

void MW::filterSyncActionsWithFilters()
{
    if (G::isLogger) G::log(__FUNCTION__);
    filterRating1Action->setChecked(filters->ratings1->checkState(0));
    filterRating2Action->setChecked(filters->ratings2->checkState(0));
    filterRating3Action->setChecked(filters->ratings3->checkState(0));
    filterRating4Action->setChecked(filters->ratings4->checkState(0));
    filterRating5Action->setChecked(filters->ratings5->checkState(0));
    filterRedAction->setChecked(filters->labelsRed->checkState(0));
    filterYellowAction->setChecked(filters->labelsYellow->checkState(0));
    filterGreenAction->setChecked(filters->labelsGreen->checkState(0));
    filterBlueAction->setChecked(filters->labelsBlue->checkState(0));
    filterPurpleAction->setChecked(filters->labelsPurple->checkState(0));
    filterLastDayAction->setChecked(filters->isOnlyMostRecentDayChecked());
}

void MW::invertFilters()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (!G::allMetadataLoaded) loadEntireMetadataCache("FilterChange");

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
    if (G::isLogger) G::log(__FUNCTION__);
    filters->uncheckAllFilters();
    filterPickAction->setChecked(false);
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
    filterLastDayAction->setChecked(false);
}

void MW::clearAllFilters()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (!G::allMetadataLoaded) loadEntireMetadataCache("FilterChange");   // rgh is this reqd
    uncheckAllFilters();
    filters->searchString = "";
    dm->searchStringChange("");
    filterChange("MW::clearAllFilters");
}

void MW::setFilterSolo()
{
    if (G::isLogger) G::log(__FUNCTION__);
    filters->setSoloMode(filterSoloAction->isChecked());
    setting->setValue("isSoloFilters", filterSoloAction->isChecked());
}

void MW::filterLastDay()
{
/*
.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    if (dm->sf->rowCount() == 0) {
        G::popUp->showPopup("No images available to filter", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }

    // if the additional filters have not been built then do an update
    if (!filters->filtersBuilt) {
        qDebug() << __FUNCTION__ << "build filters";
        launchBuildFilters();
        G::popUp->showPopup("Building filters.", 0);
        buildFilters->wait();
        G::popUp->hide();
//    if (!filters->days->childCount()) launchBuildFilters();
    }

    // if there still are no days then tell user and return
    int last = filters->days->childCount();
    if (filters->days->childCount() == 0) {
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

void MW::refine()
{
/*
    Clears refine for all rows, sets refine = true if pick = true, and clears pick
    for all rows.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    // if slideshow then do not refine
    if (G::isSlideShow) return;

    // Are there any picks to refine?
    bool isPick = false;
    for (int row = 0; row < dm->rowCount(); ++row) {
        if (dm->index(row, G::PickColumn).data() == "true") {
            isPick = true;
            break;
        }
    }

    if (!isPick) {
        G::popUp->showPopup("There are no picks to refine", 2000);
        return;
    }

    QMessageBox msgBox;
    int msgBoxWidth = 300;
    msgBox.setWindowTitle("Refine Picks");
    msgBox.setText("This operation will filter to show only your picks and then clear your picks.");
    msgBox.setInformativeText("Do you want continue?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Warning);
    QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                "QPushButton:default {background-color: rgb(68,95,118);}";
    msgBox.setStyleSheet(s);
    QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    int ret = msgBox.exec();
    if (ret == QMessageBox::Cancel) return;

    clearAllFilters();

    // clear refine = pick
    pushPick("Begin multiple select");
    for (int row = 0; row < dm->rowCount(); ++row) {
        if (dm->index(row, G::PickColumn).data() == "true") {
            // save pick history
            QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
            pushPick(fPath, "true");
            // clear picks
            dm->setData(dm->index(row, G::RefineColumn), true);
            dm->setData(dm->index(row, G::PickColumn), "false");
        }
        else dm->setData(dm->index(row, G::RefineColumn), false);
    }
    pushPick("End multiple select");

    // reset filters
    filters->uncheckAllFilters();
    filters->refineTrue->setCheckState(0, Qt::Checked);

    filterChange("MW::refine");
}

void MW::sortChangeFromAction()
{
/*
    When a sort change menu item is picked this function is called, and then redirects to
    sortChange.
*/
    if (G::isLogger) G::log(__FUNCTION__);

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
    if (sortFocalLengthAction->isChecked()) sortColumn = G::FocalLengthColumn;
    if (sortTitleAction->isChecked()) sortColumn = G::TitleColumn;
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
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, "Src: " + source);

    if (G::isInitializing || !G::isNewFolderLoaded) return;

        /*
    qDebug() << __FUNCTION__ << "source =" << source
             << "G::isNewFolderLoaded =" << G::isNewFolderLoaded
             << "G::isInitializing =" << G::isInitializing
             << "prevSortColumn =" << prevSortColumn
             << "sortColumn =" << sortColumn
             << "isReverseSort =" << isReverseSort
             << "prevIsReverseSort =" << prevIsReverseSort
                ;
//                */

    // reset sort to file name if was sorting on non-core metadata while folder still loading
    if (!G::isNewFolderLoaded && sortColumn > G::CreatedColumn) {
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
    bool doNotSort = false;
    if (sortMenuUpdateToMatchTable)
        doNotSort = true;
    if (!G::isNewFolderLoaded && sortColumn > G::CreatedColumn)
        doNotSort = true;
    if (!G::isNewFolderLoaded && sortColumn == G::NameColumn && !sortReverseAction->isChecked())
        doNotSort = true;
    if (G::isNewFolderLoaded && !sortHasChanged)
        doNotSort = true;
    if (doNotSort) return;

    // Need all metadata loaded before sorting non-core metadata
    // rgh all metadata always loaded now - change this?
    if (!G::allMetadataLoaded && sortColumn > G::CreatedColumn)
        loadEntireMetadataCache("SortChange");

    // failed to load all metadata, restore prior sort in menu and return
    if (!G::allMetadataLoaded && sortColumn > G::CreatedColumn) {
        /*
        qDebug() << __FUNCTION__ << "failed"
                 << "sortColumn =" << sortColumn
                 << "prevSortColumn =" << prevSortColumn
                 ;
//                 */
        updateSortColumn(prevSortColumn);
        return;
    }

    prevSortColumn = sortColumn;
    /*
    qDebug() << __FUNCTION__ << "succeeded"
             << "sortColumn =" << sortColumn
             << "prevSortColumn =" << prevSortColumn
             << "  Commencing sort"
             ;
//             */

    if (G::isNewFolderLoaded) {
        G::popUp->showPopup("Sorting...", 0);
    }
    else {
        setCentralMessage("Sorting images");
    }

    thumbView->sortThumbs(sortColumn, isReverseSort);

//    if (!G::allMetadataLoaded) return;

    // get the current selected item
    if (G::isNewFolderLoaded) currentRow = dm->sf->mapFromSource(currentDmIdx).row();
    else currentRow = 0;

    thumbView->iconViewDelegate->currentRow = currentRow;
    gridView->iconViewDelegate->currentRow = currentRow;
    QModelIndex idx = dm->sf->index(currentRow, 0);
    selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(currentRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache and EmbelProperties
    dm->currentFilePath = fPath;

//    if (!G::allMetadataLoaded) return;

    centralLayout->setCurrentIndex(prevCentralView);
    updateStatus(true, "", __FUNCTION__);

    // sync image cache with datamodel filtered proxy unless sort has been triggered by a
    // filter change, which will do its own rebuildImageCacheParameters
    if (source != "filterChange")
        imageCacheThread->rebuildImageCacheParameters(fPath, "SortChange");

    /* if the previous selected image is also part of the filtered datamodel then the
       selected index does not change and fileSelectionChange will not be signalled.
       Therefore we call it here to force the update to caching and icons */
//    qDebug() << __FUNCTION__ << idx.data() << "Calling fileSelectionChange(idx, idx)";
    qDebug() << __FUNCTION__ << "Calling fileSelectionChange";
    fileSelectionChange(idx, idx, __FUNCTION__);

    scrollToCurrentRow();
    G::popUp->hide();

}

void MW::updateSortColumn(int sortColumn)
{
    if (G::isLogger) G::log(__FUNCTION__);
    /*
    qDebug() << __FUNCTION__
             << "sortColumn =" << sortColumn
             << "prevSortColumn =" << prevSortColumn
             ;
    //    */

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
    if (G::isLogger) G::log(__FUNCTION__);
    toggleSortDirection(Tog::toggle);
    sortChange(__FUNCTION__);
}

void MW::toggleSortDirection(Tog n)
{
    if (G::isLogger) G::log(__FUNCTION__);
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
    if (G::isLogger) G::log(__FUNCTION__);
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

    QModelIndexList selection = selectionModel->selectedRows();
    // check if selection is entirely rating already - if so set no rating
    bool isAlreadyRating = true;
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->sf->index(selection.at(i).row(), G::RatingColumn);
        if(idx.data(Qt::EditRole) != rating) {
            isAlreadyRating = false;
        }
    }
    if (isAlreadyRating) rating = "";     // invert the label(s)

    int n = selection.count();
    if (G::useSidecar) {
        G::popUp->setProgressVisible(true);
        G::popUp->setProgressMax(n + 1);
        QString txt = "Writing to XMP sidecar for " + QString::number(n) + " images." +
                      "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
        G::popUp->showPopup(txt, 0, true, 1);
    }

    // set the rating in the datamodel
    for (int i = 0; i < n; ++i) {
        int row = selection.at(i).row();
        QModelIndex ratingIdx = dm->sf->index(row, G::RatingColumn);
        dm->sf->setData(ratingIdx, rating, Qt::EditRole);
        // update rating crash log
        QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
        updateRatingLog(fPath, rating);
        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->sf->index(selection.at(i).row(), 0);
            // is this part of a raw+jpg pair
            if(idx.data(G::DupIsJpgRole).toBool()) {
                // set rating for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupOtherIdxRole));
                row = rawIdx.row();
                ratingIdx = dm->index(row, G::RatingColumn);
                dm->setData(ratingIdx, rating, Qt::EditRole);
                // update rating crash log
                QString jpgPath  = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
                updateRatingLog(jpgPath, rating);
            }
        }
        // write to sidecar
        if (G::useSidecar) {
            dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
            metadata->writeXMP(metadata->sidecarPath(fPath), __FUNCTION__);
            G::popUp->setProgress(i+1);
        }
    }

    thumbView->refreshThumbs();
    gridView->refreshThumbs();

    // update ImageView classification badge
    updateClassification();

    // refresh the filter
    dm->sf->filterChange();

    // update filter counts
    buildFilters->updateCountFiltered();

    if (G::useSidecar) {
        G::popUp->setProgressVisible(false);
        G::popUp->hide();
    }
}

int MW::ratingLogCount()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("RatingLog");
    int count = setting->allKeys().size();
    setting->endGroup();
    return count;
}

void MW::recoverRatingLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("RatingLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
        QString pickStatus = setting->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        if (idx.isValid()) {
            QModelIndex ratingIdx = dm->sf->index(idx.row(), G::RatingColumn);
            dm->sf->setData(ratingIdx, pickStatus, Qt::EditRole);
//            qDebug() << __FUNCTION__ << pickStatus << fPath << "updated";
        }
        else {
//            qDebug() << __FUNCTION__ << fPath << "not found";
        }
    }
    setting->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearRatingLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("RatingLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        setting->remove(keys.at(i));
    }
    setting->endGroup();
}

void MW::updateRatingLog(QString fPath, QString rating)
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("RatingLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (rating == "") {
        /*
        qDebug() << __FUNCTION__ << "removing" << sKey;
        //*/
        setting->remove(sKey);
    }
    else {
        /*
        qDebug() << __FUNCTION__ << "adding" << sKey;
        //*/
        setting->setValue(sKey, rating);
    }
    setting->endGroup();
}

void MW::setColorClass()
{
/*
    Resolve the source menu action that called (could be any color class) and then set the
    color class for all the selected thumbs.
*/
    if (G::isLogger) G::log(__FUNCTION__);
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

    QModelIndexList selection = selectionModel->selectedRows();
    // check if selection is entirely label color already - if so set no label
    bool isAlreadyLabel = true;
    for (int i = 0; i < selection.count(); ++i) {
        QModelIndex idx = dm->sf->index(selection.at(i).row(), G::LabelColumn);
        if(idx.data(Qt::EditRole) != colorClass) {
            isAlreadyLabel = false;
        }
    }
    if(isAlreadyLabel) colorClass = "";     // invert the label

    int n = selection.count();
    if (G::useSidecar) {
        G::popUp->setProgressVisible(true);
        G::popUp->setProgressMax(n + 1);
        QString txt = "Writing to XMP sidecar for " + QString::number(n) + " images." +
                      "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
        G::popUp->showPopup(txt, 0, true, 1);
    }

    // update the data model
    for (int i = 0; i < n; ++i) {
        int row = selection.at(i).row();
        QModelIndex labelIdx = dm->sf->index(row, G::LabelColumn);
        dm->sf->setData(labelIdx, colorClass, Qt::EditRole);
        // update color class crash log
        QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
        updateColorClassLog(fPath, colorClass);
        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            QModelIndex idx = dm->sf->index(selection.at(i).row(), 0);
            // is this part of a raw+jpg pair
            if(idx.data(G::DupIsJpgRole).toBool()) {
                // set color class (label) for raw file row as well
                QModelIndex rawIdx = qvariant_cast<QModelIndex>(idx.data(G::DupOtherIdxRole));
                row = rawIdx.row();
                labelIdx = dm->index(row, G::LabelColumn);
                dm->setData(labelIdx, colorClass, Qt::EditRole);
                // update color class crash log
                QString jpgPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
                updateColorClassLog(jpgPath, colorClass);
            }
        }
        // write to sidecar
        if (G::useSidecar) {
            dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
            metadata->writeXMP(metadata->sidecarPath(fPath), __FUNCTION__);
            G::popUp->setProgress(i+1);
        }
    }

    thumbView->refreshThumbs();
    gridView->refreshThumbs();
    tableView->resizeColumnToContents(G::LabelColumn);

    // update ImageView classification badge
    updateClassification();

    // refresh the filter
    dm->sf->filterChange();

    // update filter counts
    buildFilters->updateCountFiltered();
    dm->sf->filterChange();

    if (G::useSidecar) {
        G::popUp->setProgressVisible(false);
        G::popUp->hide();
    }
}

int MW::colorClassLogCount()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("ColorClassLog");
    int count = setting->allKeys().size();
    setting->endGroup();
    return count;
}

void MW::recoverColorClassLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("ColorClassLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        QString fPath = keys.at(i);
        fPath.replace("🔸", "/");
        QString colorClassStatus = setting->value(keys.at(i)).toString();
        QModelIndex idx = dm->proxyIndexFromPath(fPath);
        if (idx.isValid()) {
            QModelIndex colorClassIdx = dm->sf->index(idx.row(), G::LabelColumn);
            dm->sf->setData(colorClassIdx, colorClassStatus, Qt::EditRole);
//            qDebug() << __FUNCTION__ << colorClassStatus << fPath << "updated";
        }
        else {
//            qDebug() << __FUNCTION__ << fPath << "not found";
        }
    }
    setting->endGroup();
    thumbView->refreshThumbs();
    gridView->refreshThumbs();
}

void MW::clearColorClassLog()
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("ColorClassLog");
    QStringList keys = setting->allKeys();
    for (int i = 0; i < keys.length(); ++i) {
        setting->remove(keys.at(i));
    }
    setting->endGroup();
}

void MW::updateColorClassLog(QString fPath, QString label)
{
    if (G::isLogger) G::log(__FUNCTION__);
    setting->beginGroup("ColorClassLog");
    QString sKey = fPath;
    sKey.replace("/", "🔸");
    if (label == "") {
//        qDebug() << __FUNCTION__ << "removing" << sKey;
        setting->remove(sKey);
    }
    else {
//        qDebug() << __FUNCTION__ << "adding" << sKey;
        setting->setValue(sKey, label);
    }
    setting->endGroup();
}

