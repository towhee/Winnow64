#include "Main/mainwindow.h"

/*  *******************************************************************************************

    FILTERS & SORTING

    Filters have two types:

        1. Metadata known by the operating system (name, suffix, size, modified date)
        2. Metadata read from the file by Winnow (ie cameera model, aperture, ...)

    When a folder is first selected only type 1 information is known for all the files in the
    folder unless the folder is small enough so all the files were read in one pass. Filtering
    and sorting can only occur when the filter item is known for all the files. This is
    tracked by G::allMetadataAttempted.

    Type 2 Filtration steps:

        * Make sure all metadata has been loaded for all files
        * Build the filters
        * Filter based on criteria selected
*/

void MW::filterDockTabMousePress()
{
    if (G::isLogger) G::log("MW::filterDockVisibilityChange");
    /*
    qDebug() << "MW::filterDockTabMousePress"
             << "filterDock->isVisible() =" << filterDock->isVisible()
             << "filters->filtersBuilt =" << filters->filtersBuilt
        ; //*/

    /* Clicking on the filter dock tab toggles visibility before this
    function is called, so test for the opposite. */
    if (!filters->filtersBuilt) {
        buildFiltersWhenModelReady(dm->instance);
    }
//    if (filterDock->isVisible() && !filters->filtersBuilt) {
//        buildFilters->build();
//    }
}

void MW::updateAllFilters()
{
    if (G::isLogger) G::log("MW::updateAllFilters");
    qDebug() << "MW::updateAllFilters)";
    // filters->save();
    buildFilters->build();
    // filters->restore();
    // filterChange("updateAllFilters");
}

void MW::updateFilterMenu(QString source)
{
/*
    Signalled from BuildFilters::updateCategoryItems and
    BuildFilters::updateFilteredCounts.

    After a filters recount enable/disable filter menu items based on whether they
    are still in the proxy datamodel (dm->sf):

    Disable filter menu items that are not in the filter menu and have a sf count = 0
    Enable filter menu items that are in the filter menu and have a sf count > 0
*/
    if (G::isLogger) G::log("MW::updateFilterMenu");
    // qDebug() << "MW::updateFilterMenu source =" << source;

    auto f = [this](const QString &name) {
        return filters->isPredefinedNonZeroCount(name);
    };

    // picks
    filterPickAction->setEnabled(f("Picked"));

    // ratings
    filterRating1Action->setEnabled(f("1"));
    filterRating2Action->setEnabled(f("2"));
    filterRating3Action->setEnabled(f("3"));
    filterRating4Action->setEnabled(f("4"));
    filterRating5Action->setEnabled(f("5"));

    // color labels
    filterRedAction->setEnabled(f("Red"));
    filterYellowAction->setEnabled(f("Yellow"));
    filterGreenAction->setEnabled(f("Green"));
    filterBlueAction->setEnabled(f("Blue"));
    filterPurpleAction->setEnabled(f("Purple"));
}

void MW::filterChange(QString source)
{
/*
    All filter changes should be routed to here as a central clearing house.

    - the datamodel instance is incremented
    - the datamodel proxy filter is refreshed
    - the filter panel counts are updated
    - pick, rating and label 0 count disabled, rest enabled
    - the current index is updated
    - any prior selection that is still available is set
    - the image cache is rebuilt to match the current filter
    - the thumb and grid first/last/thumbsPerPage parameters are recalculated
      and icons are loaded if necessary.

    Save and recover selection is not required for filter operations.  It is
    required for sorting operations.
*/

    QString srcFun = "MW::filterChange";
    if (G::isLogger || G::isFlowLogger) G::log("MW::filterChange  Src: ", source);
    // qDebug() << "MW::filterChange" << "called from:" << source;

    // ignore if new folder is being loaded
    if (!G::allMetadataAttempted) {
         return;
    }

    if (G::stop) return;

    /*  BOTH SCOPES FILTER THE PROXY NOW, and this gate is gone.

        It existed because the Catalog tree used to hold the CATALOG's values, queried
        from the index rather than read from the loaded rows, and its checkboxes described
        a search that RELOADED the model rather than a filter over it. Running the proxy
        over values the datamodel had never heard of would have narrowed the loaded set to
        nothing.

        That is no longer what a catalog scope is: the whole catalog is loaded, so the
        datamodel's own categories ARE the library's, and checking one means exactly what
        it means in a folder. The visible failure of the old arrangement was that
        filtering on a day rebuilt the tree from that day alone -- every other value
        vanished instead of the category head turning yellow. */

    /*  TIMED, because this now runs in catalog scope too and a catalog scope is 43,000
        rows: an invalidate walks every one of them through filterAcceptsRow, and anything
        that calls this repeatedly during a rebuild would be a stall the early return used
        to hide. */
    QElapsedTimer fcTimer;
    /*  PHASE TIMER, because "MW::filterChange took 4 seconds" does not say which of the
        six things it does was the four seconds. Each phase reports the time SINCE the
        previous one, so the numbers add up to the total below. */
    QElapsedTimer phTimer;
    QStringList phases;
    auto phase = [&](const QString &name) {
        if (!G::isPerfProbe) return;
        phases << QString("%1=%2").arg(name).arg(phTimer.restart());
    };
    if (G::isPerfProbe) {
        fcTimer.start();
        phTimer.start();
        dm->probeSnapRebuilds = 0;
        dm->probeSnapNs = 0;
    }

    /*  ONE ACCESSIBILITY REBUILD FOR THE WHOLE FILTER CHANGE, not one per row-insertion
        block and one per deferred view layout. See G::A11ySuspend for the measurement and
        for why the guard has to cover scrollToCurrentRowIfNotVisible as well: the views'
        deferred layout -- with the accessibility notification it carries -- is executed
        from inside that function, by the visualRect and indexAt calls that measure cell
        visibility. */
    G::A11ySuspend a11ySuspend;

    // increment the dm->instance.  This is necessary to ignore any updates to ImageCache
    // and MetaRead for the prior datamodel filter.
    dm->newInstance();

    // if filter change source is the filter panel then sync menu actions isChecked property
    if (source == "Filters::itemClickedSignal") syncActionsWithFilters();

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // prevent unwanted fileSelectionChange()
    isFilterChange = true;

    // save existing selection
    sel->save("MW::filterChange");

    /* refresh the proxy sort/filter, which updates the selectionIndex, which
    triggers a scroll event and MetaRead updates the icons and thumbnails */

    dm->sf->suspend(false, "MW::filterChange");
    dm->sf->filterChange("MW::filterChange"); // crash (removed wait in SortFilter::filterChange)
    phase("sfFilter");

    // update filter panel image count by filter item
    buildFilters->update();
    phase("buildFilters");

    /*  RECOVER THE SORT ONLY IF IT NEEDS RECOVERING. This called sortChange
        unconditionally, and sortChange re-sorts through QSortFilterProxyModel::sort,
        which does NOT early-out on a column and order it is already sorted by -- it
        re-sorts all 43,000 rows and emits layoutChanged to three views, then saves and
        recovers the selection and refreshes the icons on top. Measured at 0.6 to 3.9
        seconds per filter change, for an order that had not changed.

        NOTHING WAS RECOVERING. Filtering does not disturb the sort: the proxy runs with
        dynamicSortFilter TRUE in steady state (it is turned off only for the duration of
        a batched folder load and put back by restoreProxySortAfterLoad), so the rows
        invalidateRowsFilter brings back are inserted in sorted position as they arrive.
        The one thing sortChange did that the phases after here rely on is currentSfRow,
        which is cheap to keep on its own. A sort the proxy is genuinely NOT holding --
        after a load, a workspace, or a menu sort -- still goes through sortChange as
        before, because the guard tests what the proxy actually has. */
    const Qt::SortOrder wantOrder = isReverseSort ? Qt::DescendingOrder : Qt::AscendingOrder;
    if (dm->sf->sortColumn() == sortColumn && dm->sf->sortOrder() == wantOrder) {
        dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    }
    else {
        sortChange("filterChange");
    }
    phase("sort");

    // allow fileSelectionChange()
    isFilterChange = false;

    // update the status panel filtration status
    updateStatusBar();
    updateStatus(true, "", "MW::filterChange");
    phase("status");

    // if filter has eliminated all rows so nothing to show
    if (!dm->sf->rowCount()) {
        nullFiltration();
        QApplication::restoreOverrideCursor();
         if (G::isLogger || G::isFlowLogger)
            G::log("MW::filterChange", "Null result");
        return;
    }

    if (thumbView && thumbView->isVisible()) {
        thumbView->refreshIcons(srcFun);
        thumbView->scrollToRow(dm->currentSfRow, srcFun);
    }
    if (gridView && gridView->isVisible()) {
        gridView->refreshIcons(srcFun);
        gridView->scrollToRow(dm->currentSfRow, srcFun);
    }
    phase("refreshIcons");
    /*  THE WORKERS ARE ABOUT TO BE TOLD, so the snapshot they read has to be current
        before it is queued rather than on the next turn of the event loop. The rebuild is
        coalesced now (see DataModel::scheduleProxySnapshotRebuild), which is what took
        this phase from 348 walks of the whole proxy to one. */
    dm->flushProxySnapshot();
    phase("snapshot");

    /*  Sync the datamodel instance -- QUEUED, like every other caller of initialize.
        It was a DIRECT call, which meant the GUI thread cleared rowsReading,
        videoRowsReading and readSuccessThisCycle while metaReadThread was inside
        MetaRead::needToRead reading those very sets: a QSet cleared under a concurrent
        contains() is a freed hash pointer, and the crash landed in needToRead with a null
        dereference. Nothing here depends on it having run before this function returns --
        the setStartRow that acts on it is itself queued, behind this. */
    QMetaObject::invokeMethod(metaRead, "initialize", Qt::QueuedConnection,
                              Q_ARG(QString, srcFun));

    /*  IS THE CURRENT IMAGE STILL IN THE FILTER? If not, fall back to a row that
        EXISTS -- and the clamp is the whole point, because every candidate here can be -1.

        THIS WAS THE "FILTER THE CATALOG AND NO THUMBNAILS PAINT" DEFECT. mapFromSource is
        invalid whenever the filter removed the current image, which is the ordinary case.
        The fallback then read dm->currentSfIdx, a PERSISTENT index into the proxy -- and
        SortFilter::filterChange invalidates rather than re-mapping (see the note on
        invalidate() above), so after the filter it reads back as row -1. Since -1 is less
        than sfRows, the old test took the first branch and built sf->index(-1, 0): an
        INVALID index. Selection::select rejects one -- it warns, beeps and returns without
        selecting -- so nothing emitted fileSelectionChange, nothing dispatched MetaRead
        for the new visible window, and the thumbnails never loaded. A click in thumbView
        or a switch to gridView and back made them appear because both dispatch a read.
        The probe line said it out loud every time: "Selection::select(QModelIndex)
        sfIdx = QModelIndex(-1,-1) invalid."

        dm->currentSfRow is the maintained plain int and is tried first; row 0 is the last
        resort. A filtration with no rows at all has already returned via nullFiltration. */
    QModelIndex newSfIdx = dm->sf->mapFromSource(dm->currentDmIdx);
    const int sfRows = dm->sf->rowCount();
    if (!newSfIdx.isValid() && sfRows > 0) {
        int row = dm->currentSfRow;
        if (row < 0) row = dm->currentSfIdx.row();
        if (row < 0) row = 0;
        if (row >= sfRows) row = sfRows - 1;
        newSfIdx = dm->sf->index(row, 0);
    }

    // rebuild imageCacheList and update priorities in image cache
    QString fPath = newSfIdx.data(G::PathRole).toString();
    if (!G::removingRowsFromDM)
        emit imageCacheFilterChange(fPath, "MW::filterChange");
    phase("imageCache");

    // // // clear selection and update datamodel current index
    // sel->select(newSfIdx, Qt::NoModifier, "MW::filterChange");

    // Recover full selection if filter didn't eliminate any rows; otherwise fall
    // back to selecting the current single item.
    if (dm->rowCount() == dm->sf->rowCount()) {
        sel->recover("MW::filterChange");
    } else {
        sel->select(newSfIdx, Qt::NoModifier, "MW::filterChange");
    }
    phase("select");

    // only scroll if filtration has changed visible cells in thumbView
    scrollToCurrentRowIfNotVisible();
    phase("scroll");

    /*  TELL THE LOADER, UNCONDITIONALLY. After a filter change the rows under the visible
        window are DIFFERENT IMAGES, and most of them have no icon yet -- only MetaRead can
        fix that, and until now nothing here asked it to. The dispatch was a side effect of
        the selection, and BOTH branches above can be silent:

            - Selection::recover uses QItemSelectionModel::NoUpdate deliberately (see its
              comment, and "Selection recovery and the central view"), so it emits no
              fileSelectionChange at all. That is the branch taken when the filter removed
              nothing -- UNCHECKING a filter, where every row on screen changes.
            - Selection::select is a no-op when the row it is given is already current.

        Symptom when it is silent: uncheck a filter and the thumb strip paints only the
        images that happened to have icons already -- three of them, around the current
        row -- while the rest stay empty until a click or a view switch dispatches a read.
        Placed AFTER scrollToCurrentRowIfNotVisible because that is where the icon range
        this dispatch reads is established (MW::updateIconRange -> setIconRange), and the
        snapshot is flushed first because the worker reads it on the other thread and this
        function does not return to the event loop before the invoke. One queued call per
        user action; rows that already have what they need are skipped by needToRead. */
    if (!G::isInitializing && G::useReadMeta) {
        const int startRow = qBound(0, dm->currentSfRow, dm->sf->rowCount() - 1);
        dm->flushProxySnapshot();
        QMetaObject::invokeMethod(metaRead, "setStartRow", Qt::QueuedConnection,
                                  Q_ARG(int, startRow),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, QString("MW::filterChange")));
    }
    phase("dispatch");

    QApplication::restoreOverrideCursor();

    if (G::isPerfProbe)
        qDebug().noquote() << "[PERF] MW::filterChange" << fcTimer.elapsed() << "ms  src ="
                           << source << " dmRows =" << dm->rowCount()
                           << " sfRows =" << dm->sf->rowCount()
                           << " a11ySuspended =" << a11ySuspend.suspended()
                           << " snapshots =" << dm->probeSnapRebuilds
                           << "(" << QString::number(dm->probeSnapNs / 1.0e6, 'f', 1) << "ms)"
                           << " phases(ms):" << phases.join(" ");
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

void MW::syncActionsWithFilters()
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

bool MW::isSortFilter()
{
    if (G::isLogger) G::log("MW::isSortFilter");
    bool ret = false;
    if (dm->rowCount() > dm->sf->rowCount()) ret = true;
    if (sortColumn > G::NameColumn) ret = true;
    return ret;
}

void MW::invertFilters()
{
/*

*/
    if (G::isLogger) G::log("MW::invertFilters");
    if (!G::allMetadataAttempted) loadEntireMetadataCache("FilterChange");

    if (dm->rowCount() == 0) {
        G::popup->showPopup("No images available to invert filtration", 2000);
        filterLastDayAction->setChecked(false);
        return;
    }
    if (dm->rowCount() == dm->sf->rowCount()) {
        G::popup->showPopup("No filters assigned - null inversion would result", 2000);
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
}

void MW::clearAllFilters()
{
    if (G::isLogger) G::log("MW::clearAllFilters");
    if (!G::allMetadataAttempted) loadEntireMetadataCache("FilterChange");   // rgh is this reqd
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
        G::popup->showPopup("No images available to filter", 2000);
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
        G::popup->showPopup("No days are available to filter", 2000);
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
    if (sortFileSizeAction->isChecked()) sortColumn = G::ByteSizeColumn;        // core
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

    qDebug() << "MW::sortChangeFromAction sortColumn =" << sortColumn;

    sortChange("Action");
}

void MW::sortChange(QString source)
{
/*
    Triggered by a menu sort item, a new folder or a new workspace.

    The initial sort is always by file name.

    The sort order (ascending or descending) can be set by the menu, the button icon
    on the statusbar or a workspace change.
*/
    if (G::isLogger || G::isFlowLogger)
        G::log("MW::sortChange",  "Src = " + source);
    // qDebug() << "MW::sortChange  Src:" << source;

    if (G::isInitializing || !G::allMetadataAttempted || sortMenuUpdateToMatchTable) {
        return;
    }

    setCentralMessage("Sorting images");

    // save selection prior to sorting
    sel->save("MW::sortChange");

    // do the sort
    // qDebug() << "MW::sortChange  sortColumn =" << sortColumn << isReverseSort;
    thumbView->sortThumbs(sortColumn, isReverseSort);

    // recover selection
    sel->recover("MW::sortChange");

    // get the current selected item
    dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();

    // refresh icons in view
    if (thumbView->isVisible()) thumbView->refreshIcons("MW::sortChange");
    if (gridView->isVisible()) gridView->refreshIcons("MW::sortChange");

    // update view delegates
    thumbView->iconViewDelegate->currentRow = dm->currentSfRow;
    gridView->iconViewDelegate->currentRow = dm->currentSfRow;

    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(dm->currentSfRow, 0).data(G::PathRole).toString();

    centralLayout->setCurrentIndex(prevCentralView);
    updateStatus(true, "", "MW::sortChange");

    /* sync image cache with datamodel filtered proxy unless sort has been triggered by a
       filter change, which will do its own rebuildImageCacheParameters */
    if (source != "filterChange")
        emit imageCacheFilterChange(fPath, "SortChange");

    scrollToCurrentRowIfNotVisible();
    G::popup->reset();
}

void MW::sortReverse()
/*
    Experiment to reverse sort on the file name while a new folder is loading.
*/
{
    thumbView->sortThumbs(G::NameColumn, isReverseSort);

    // get the current selected item
    if (G::allMetadataAttempted) dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    else dm->currentSfRow = 0;

    thumbView->iconViewDelegate->currentRow = dm->currentSfRow;
    gridView->iconViewDelegate->currentRow = dm->currentSfRow;
    QModelIndex idx = dm->sf->index(dm->currentSfRow, 0);
    dm->selectionModel->setCurrentIndex(idx, QItemSelectionModel::Current);
    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(dm->currentSfRow, 0).data(G::PathRole).toString();
    // also update datamodel, used in MdCache and EmbelProperties
    dm->currentFilePath = fPath;
    sel->select(idx, Qt::NoModifier,"MW::sortReverse");

    scrollToCurrentRowIfNotVisible();
}

void MW::updateSortColumn(int sortColumn)
{
    if (G::isLogger) G::log("MW::updateSortColumn");

    if (sortColumn == 0) sortColumn = G::NameColumn;

    if (sortColumn == G::NameColumn) sortFileNameAction->setChecked(true);
    if (sortColumn == G::TypeColumn) sortFileTypeAction->setChecked(true);
    if (sortColumn == G::ByteSizeColumn) sortFileSizeAction->setChecked(true);
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

// void MW::toggleSortDirectionClick()
// {
// /*
//     This is called by connect signals from the menu action and the reverse sort button.  The
//     call is redirected to toggleSortDirection, which has a parameter which is not supported
//     by the action and button signals.
// */
//     if (G::isLogger) G::log("MW::toggleSortDirectionClick");
//     toggleSortDirection(Tog::toggle);
//     sortChange("MW::toggleSortDirectionClick");
//     // Experiment to reverse sort on the file name while a new folder is loading.
//     // sortReverse();
// }

// void MW::toggleSortDirection(Tog n)
// {
//     if (G::isLogger) G::log("MW::toggleSortDirection");
//     if (prevIsReverseSort == isReverseSort)
//     prevIsReverseSort = isReverseSort;
//     if (n == Tog::toggle) isReverseSort = !isReverseSort;
//     if (n == Tog::off) isReverseSort = false;
//     if (n == Tog::on) isReverseSort = true;
//     if (isReverseSort) {
//         sortReverseAction->setChecked(true);
//         reverseSortBtn->setIcon(QIcon(":/images/icon16/Z-A.png"));
//     }
//     else {
//         sortReverseAction->setChecked(false);
//         reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));
//     }
// }

void MW::setRating()
{
/*
    Resolve the source menu action that called (could be any rating) and then set the
    rating for all the selected thumbs.
*/
    if (G::isLogger) G::log("MW::setRating");
    // qDebug() << "MW::setRating";
    // do not set rating if slideshow is on
    if (G::isSlideShow) return;

    // make sure classification badges are visible
    if (!isRatingBadgeVisible) {
        ratingBadgeVisibleAction->setChecked(true);
        isRatingBadgeVisible = true;
        thumbView->refreshIcons("MW::setRating");
        gridView->refreshIcons("MW::setRating");
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
        // update rating crash log
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::PathRole).toString();
        updateRatingLog(fPath, rating);
        // update datamodel
        QModelIndex ratingIdx = dm->index(dmRow, G::RatingColumn);
        emit setValDm(dmRow, G::RatingColumn, rating, dm->instance, src,
                      Qt::EditRole);
        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            // is this part of a raw+jpg pair
            int rowDup = dm->isDupJpg(dmRow) ? dm->dupOtherRow(dmRow) : -1;
            if (rowDup >= 0) {
                // update rating crash log
                QString jpgPath  = dm->index(rowDup, G::PathColumn).data(G::PathRole).toString();
                updateRatingLog(jpgPath, rating);
                // set rating for raw file row as well
                emit setValDm(rowDup, G::RatingColumn, rating, dm->instance, src, Qt::EditRole);
            }
        }
        // write to sidecar
        dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
        metadata->writeXMP(fPath, "MW::setRating");
        // update _Rating (used to check what metadata has changed in metadata->writeXMP)
        QModelIndex _ratingIdx = dm->index(dmRow, G::_RatingColumn);
        emit setValDm(dmRow, G::_RatingColumn, rating, dm->instance, src, Qt::EditRole);
        /*  And the catalog, so a rating set in Catalog scope is searchable
            without having to re-open the folder -- see MW::updateCatalogForRow. */
        updateCatalogForRow(dmRow);
        G::popup->setProgress(i+1);
    }

    // update thumbnail appearance to show classification
    thumbView->refreshIcons("MW::setRating");
    gridView->refreshIcons("MW::setRating");

    /* must execute in order:
       - suspend proxy updates
       - update filter category
       - if cat checked clear selection
       - update proxy (filterChange) */

    // update category list in filters
    dm->sf->suspend(true, "MW::setRating");
    buildFilters->updateCategory(BuildFilters::RatingEdit);
    filterChange("MW::setRating");

    // update ImageView classification badge
    updateClassification();

    // auto advance
    if (G::autoAdvance) sel->next();
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
            int sfRow = idx.row();
            emit setValSf(sfRow, G::RatingColumn, pickStatus, dm->instance,
                          src, Qt::EditRole);
        }
        else {
            // qDebug() << "MW::recoverRatingLog" << fPath << "not found";
        }
    }
    settings->endGroup();
    thumbView->refreshIcons("MW::recoverRatingLog");
    gridView->refreshIcons("MW::recoverRatingLog");
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

void MW::setColorClassForRow(int sfRow, QString colorClass) {
    QString srcFun = "MW::setColorClassForRow";
    qDebug() << srcFun << sfRow << colorClass;
    emit setValSf(sfRow, G::LabelColumn, colorClass,
                  dm->instance, "MW::setColorClassForRow",
                  Qt::EditRole);
    QString color = dm->sf->index(sfRow, G::LabelColumn).data().toString();
    qDebug() << srcFun << "color =" << color;
    thumbView->refreshIcons("MW::setColorClassForRow");
    gridView->refreshIcons("MW::setColorClassForRow");
    dm->sf->suspend(true, "MW::setColorClass");
    buildFilters->updateCategory(BuildFilters::LabelEdit);
    filterChange("MW::setColorClass"); // sets dm->sf->suspend = false
    // update ImageView classification badge
    updateClassification();
    // write to sidecar
    QString fPath = dm->pathFromProxyRow(sfRow);
    dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
    metadata->writeXMP(fPath, "MW::setColorClass");
    // update _Label (used to check what metadata has changed in metadata->writeXMP)
    emit setValSf(sfRow, G::_LabelColumn, colorClass, dm->instance, srcFun,
                  Qt::EditRole);
    updateCatalogForRow(dm->modelRowFromProxyRow(sfRow));
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
        thumbView->refreshIcons("MW::setColorClass");
        gridView->refreshIcons("MW::setColorClass");
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

    /* copy selection to list of dm rows (proxy filter changes during iteration when
       change datamodel)  */
    QList<int> rows;
    for (int i = 0; i < n; ++i) {
        int dmRow = dm->modelRowFromProxyRow(selection.at(i).row());
        rows.append(dmRow);
    }

    // update the data model
    QString src = "MW::setColorClass";
    for (int i = 0; i < n; ++i) {
        int dmRow = rows.at(i);
        // update color class crash log
        QString fPath = dm->index(dmRow, G::PathColumn).data(G::PathRole).toString();
        updateColorClassLog(fPath, colorClass);
        // update datamodel
        QModelIndex labelIdx = dm->index(dmRow, G::LabelColumn);
        emit setValDm(dmRow, G::LabelColumn, colorClass, dm->instance, src,
                      Qt::EditRole);
        // check if combined raw+jpg and also set the rating for the hidden raw file
        if (combineRawJpg) {
            // is this part of a raw+jpg pair
            int rowDup = dm->isDupJpg(dmRow) ? dm->dupOtherRow(dmRow) : -1;
            if (rowDup >= 0) {
                // update color class crash log
                /* rowDup is a DATAMODEL row -- and when combineRawJpg is on the
                   hidden raw row is filtered out of the proxy, so reading it
                   through dm->sf named a different image (or none). */
                QString jpgPath = dm->index(rowDup, G::PathColumn).data(G::PathRole).toString();
                updateColorClassLog(jpgPath, colorClass);
                // set color class (label) for raw file row as well
                QString src = "MW::setColorClass";
                emit setValDm(rowDup, G::LabelColumn, colorClass, dm->instance, src,
                              Qt::EditRole);
            }
        }
        // write to sidecar
        dm->imMetadata(fPath, true);    // true = update metadata->m struct for image
        metadata->writeXMP(fPath, "MW::setColorClass");
        // update _Label (used to check what metadata has changed in metadata->writeXMP)
        emit setValDm(dmRow, G::_LabelColumn, colorClass, dm->instance, src,
                      Qt::EditRole);
        updateCatalogForRow(dmRow);
        G::popup->setProgress(i+1);
    }

    // update thumbnail appearance to show classification
    thumbView->refreshIcons("MW::setColorClass");
    gridView->refreshIcons("MW::setColorClass");

    /* must execute in order:
       - suspend proxy updates
       - update filter category
       - if cat checked clear selection
       - update proxy (filterChange)
    */

    // update category list in filters
    dm->sf->suspend(true, "MW::setColorClass");
    buildFilters->updateCategory(BuildFilters::LabelEdit);
    filterChange("MW::setColorClass");

    // // // was this rating filtered
    // if (filters->isLabelChecked(colorClass)) {
    //     sel->clear();
    //     filterChange("MW::setColorClass");
    // }

    // update ImageView classification badge
    updateClassification();

    // auto advance
    if (G::autoAdvance) sel->next();
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
        int sfRow = idx.row();
        QString src = "MW::recoverColorClassLog";
        if (idx.isValid()) {
            emit setValSf(sfRow, G::LabelColumn, colorClassStatus, dm->instance,
                          src, Qt::EditRole);
        }
        else {
            // qDebug() << "MW::recoverColorClassLog" << fPath << "not found";
        }
    }
    settings->endGroup();
    thumbView->refreshIcons("MW::recoverColorClassLog");
    gridView->refreshIcons("MW::recoverColorClassLog");
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

    /*  WITH THE FIND DOCK, F2 is the Folders half of the F2 / Shift+F2 pair: the
        same shared search box as Shift+F2, switched to the Folders scope (see
        MW::showCatalogDock for the other half).

        It used to open an editor on filters->searchTrue instead. But
        Filters::showAllCategories HIDES the Search category whenever
        G::useFindDock -- the box is the same fact shown twice -- so F2 was
        opening an editor on an item the user could not see, and the box it
        should have focused was never touched. The searchTrue item still exists
        as the STORAGE the proxy predicate reads via Filters::setSearchText;
        it is just no longer the thing the user types into. */
    if (G::useFindDock) {
        if (!findPanel) return;
        setScope(G::Scope::Folders, "MW::searchTextEdit");
        findPanel->focusSearch();
        return;
    }

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

