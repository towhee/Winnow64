#include "Main/mainwindow.h"

/*
    Builds the "Decode Raw is On" toolbar icon: a 2px white border surrounding a
    2x2 Bayer-style quadrant grid (top-left red, top-right green, bottom-left
    green, bottom-right blue).  Rendered at 2x for crisp display on Retina.
*/
static QIcon makeUseRawOnIcon()
{
    const int size = 16;                     // 16x16 px to match toolbar icons
    const int border = 2;                    // 2px white border

    QPixmap pm(size, size);
    pm.fill(Qt::white);

    const int inner = size - 2 * border;
    const int half = inner / 2;
    const int x0 = border;
    const int x1 = border + half;
    const int y0 = border;
    const int y1 = border + half;

    QPainter p(&pm);
    p.fillRect(QRect(x0, y0, half, half), QColor(255,   0,   0));  // top-left  red
    p.fillRect(QRect(x1, y0, half, half), QColor(  0, 255,   0));  // top-right green
    p.fillRect(QRect(x0, y1, half, half), QColor(  0, 255,   0));  // bottom-left  green
    p.fillRect(QRect(x1, y1, half, half), QColor(  0,   0, 255));  // bottom-right blue
    p.end();

    return QIcon(pm);
}

void MW::updateStatus(bool keepBase, QString s, QString source)
{
/*
    Reports status information on the status bar and in InfoView.  If keepBase = true
    then ie "1 of 80   60% zoom   2.1 MB picked" is prepended to the status message s.
*/
    if (!G::useUpdateStatus) return;

    QString fun = "MW::updateStatus";
    if (G::isLogger)
        G::log(fun);

    // qDebug() << fun << keepBase << s << source;

    // check if null filter
    if (keepBase && dm->sf->rowCount() == 0) {
        statusLabel->setText("");
        if (G::useInfoView)  {
        QStandardItemModel *k = infoView->ok;
            k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->SelectedRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), "");
        }
        updateStatusBar();
        return;
    }

    QString status;
    QString fileCount = "";
    QString zoomPct = "";
    QString base = "";
    QString spacer = "   ";
    QString raw = "";

    // update G::availableMemory
#ifdef Q_OS_WIN
    Win::availableMemory();
    double availMemGB = static_cast<double>(G::availableMemoryMB) / 1024;
    QString mem = QString::number(availMemGB, 'f', 1) + " GB";
#endif

    // show embellish if active
    if (G::isEmbellish) {
        base = "<font color=\"red\">" + embelProperties->templateName + "</font>"
                + "&nbsp;" + "&nbsp;" + "&nbsp;";
    }
    else base = "  ";

    // define base status text
    if (keepBase) {
        // Zoom
        if (G::mode == "Loupe" || G::mode == "Compare" || G::mode == "Embel") {
            base += "Zoom: " + getZoom();
        }
        // Position
        base += spacer + "Pos: " + getPosition();
        if (source != "nextSlide") {
            QString s = QString::number(sel->count());
            // Selected count
            base += spacer +" Selected: " + s;
            // Picked count
            base += spacer + "Picked: " + getPicked();
            // Raw rendering
            base += spacer + getRawRendered();
        }

        QString preBase = "";
        if (G::isStressTest) {
            preBase = "<font color=\"red\"><b>PRESS ESC TO STOP STRESS TEST</b></font>  ";
            preBase += QString::number(stressSecToGoInFolder) + " seconds.\t";
        }
        status = preBase + base;
    }

    if (!keepBase) status = s;

    statusLabel->setText(status);

    // status label tooltip
    QString tip;
    if (G::isEmbellish) {
        tip = "Embellish is active.";
    }
    statusLabel->setToolTip(tip);

    /*
    status = " " + base + s + spacer + ms;
    qDebug() << "Status:" << status;
    */

    // update InfoView - flag updates so itemChanged is ignored in MW::metadataChanged
    if (G::useInfoView)  {
        infoView->ignoreDataChange = true;
        QStandardItemModel *k = infoView->ok;
        if (keepBase) {
            k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), getPosition());
            k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), getZoom());
            k->setData(k->index(infoView->SelectedRow, 1, infoView->statusInfoIdx), getSelectedFileSize());
            k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), getPicked());
        }
        else {
            k->setData(k->index(infoView->PositionRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->ZoomRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->SelectedRow, 1, infoView->statusInfoIdx), "");
            k->setData(k->index(infoView->PickedRow, 1, infoView->statusInfoIdx), "");
        }
        infoView->ignoreDataChange = false;
    }


}

void MW::clearStatus()
{
    if (G::isLogger) G::log("MW::clearStatus");
    statusLabel->setText("");
}

void MW::updateStatusBar()
{
    if (G::isLogger) G::log("MW::updateStatusBar");

    if (G::modifySourceFiles) {
        modifyImagesBtn->setIcon(QIcon(":/images/icon16/delta_red_16.png"));
        modifyImagesBtn->setToolTip("Modify image files is On.  Click to toggle on/off");
    }
    else {
        modifyImagesBtn->setIcon(QIcon(":/images/icon16/delta_bw_16.png"));
        modifyImagesBtn->setToolTip("Modify image files is Off.  Click to toggle on/off");
    }

    if (G::includeSidecars) {
        includeSidecarsToggleBtn->setIcon(QIcon(":/images/icon16/sidecar.png"));
        includeSidecarsToggleBtn->setToolTip("Include sidecars when dragging is On.  Click to toggle on/off");
    }
    else {
        includeSidecarsToggleBtn->setIcon(QIcon(":/images/icon16/nosidecar.png"));
        includeSidecarsToggleBtn->setToolTip("Include sidecars when dragging is Off.  Click to toggle on/off");
    }

    if (G::colorManage) {
        colorManageToggleBtn->setIcon(QIcon(":/images/icon16/rainbow1.png"));
        colorManageToggleBtn->setToolTip("Color Manage is On.  Click to toggle on/off");
    }
    else {
        colorManageToggleBtn->setIcon(QIcon(":/images/icon16/norainbow1.png"));
        colorManageToggleBtn->setToolTip("Color Manage is Off.  Click to toggle on/off");
    }

    if (panFocusToggleAction->isChecked()) {
        panToFocusToggleBtn->setIcon(QIcon(":/images/icon16/target.png"));
        panToFocusToggleBtn->setToolTip(
            "Pan to predicted focus point is On.  Click to toggle on/off.  "
            "Shortcut is 'B'"
        );
    }
    else {
        panToFocusToggleBtn->setIcon(QIcon(":/images/icon16/target_bw.png"));
        panToFocusToggleBtn->setToolTip(
            "Pan to predicted focus point is Off.  Click to toggle on/off.  "
            "Shortcut: B"
        );
    }

    if (G::useRaw) {
        useRawBtn->setIcon(makeUseRawOnIcon());
        useRawBtn->setToolTip("Decode Raw is On (demosaic sensor data).  Click to toggle on/off");
    }
    else {
        useRawBtn->setIcon(QIcon(":/images/icon16/raw_bw.png"));
        useRawBtn->setToolTip("Decode Raw is Off (show embedded preview).  Click to toggle on/off");
    }

    if (sortReverseAction->isChecked()) {
        reverseSortBtn->setIcon(QIcon(":/images/icon16/Z-A.png"));
        reverseSortBtn->setToolTip(
            "Reverse sort is On.  Click to toggle on/off.  "
            "Shortcut: Opt/Alt + S"
        );
    }
    else {
        reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));
        reverseSortBtn->setToolTip(
            "Reverse sort is Off.  Click to toggle on/off.  "
            "Shortcut: Opt/Alt + S"
        );
    }

    /* Disable Combine Raw/JPG toggling while a folder is loading (rows exist but
       metadata is still being read); toggling is allowed when no folder is loaded. */
    bool rawJpgEnabled = !(dm->rowCount() > 0 && !G::allMetadataAttempted);
    rawJpgStatusBtn->setEnabled(rawJpgEnabled);
    combineRawJpgAction->setEnabled(rawJpgEnabled);

    if (combineRawJpg) {
        rawJpgStatusBtn->setIcon(QIcon(":/images/icon16/link.png"));
        rawJpgStatusBtn->setToolTip(
            rawJpgEnabled
            ? "Combine Raw/JPG is On.  Click to toggle on/off.  Shortcut: Opt/Alt + J"
            : "Combine Raw/JPG is On.  Disabled while the folder is loading."
        );
    }
    else {
        rawJpgStatusBtn->setIcon(QIcon(":/images/icon16/nolink.png"));
        rawJpgStatusBtn->setToolTip(
            rawJpgEnabled
            ? "Combine Raw/JPG is Off.  Click to toggle on/off.  Shortcut: Opt/Alt + J"
            : "Combine Raw/JPG is Off.  Disabled while the folder is loading."
        );
    }

    G::isFilter = filters->isAnyFilter();
    filterStatusLabel->setVisible(G::isFilter);

    // deprecated when utilized opt + click on FSTree folder
    // subfolderStatusLabel->setVisible(dm->subFolderImagesLoaded);

    slideShowStatusLabel->setVisible(G::isSlideShow);

    /* The metadata and image cache running lights report background caching, which is a
       Preview mode activity (read-ahead over the folder).  Develop works on the single
       current image with the read-ahead trimmed, so the lights are meaningless there:
       show them in Preview, hide them in Develop. */
    bool inPreview = G::operationMode == G::OperationMode::Preview;
    metadataThreadRunningLabel->setVisible(inPreview);
    imageThreadRunningLabel->setVisible(inPreview);
}

int MW::availableSpaceForProgressBar()
{
    if (G::isLogger) G::log("MW::availableSpaceForProgressBar");
    int w = 0;
    int s = statusBar()->layout()->spacing();
    if (statusBarSpacer1->isVisible()) w += s + statusBarSpacer1->width();
    if (modifyImagesBtn->isVisible()) w += s + modifyImagesBtn->width();
    if (colorManageToggleBtn->isVisible()) w += s + colorManageToggleBtn->width();
    if (panToFocusToggleBtn->isVisible()) w += s + panToFocusToggleBtn->width();
    if (reverseSortBtn->isVisible()) w += s + reverseSortBtn->width();
    if (useRawBtn->isVisible()) w += s + useRawBtn->width();
    if (rawJpgStatusBtn->isVisible()) w += s + rawJpgStatusBtn->width();
    if (filterStatusLabel->isVisible()) w += s + filterStatusLabel->width();
    if (subfolderStatusLabel->isVisible()) w += s + subfolderStatusLabel->width();
    if (slideShowStatusLabel->isVisible()) w += s + slideShowStatusLabel->width();
    if (statusLabel->isVisible()) w += s + statusLabel->width();
    if (metadataThreadRunningLabel->isVisible()) w += s + metadataThreadRunningLabel->width();
    if (imageThreadRunningLabel->isVisible()) w += s + imageThreadRunningLabel->width();
    if (statusBarSpacer->isVisible()) w += s + statusBarSpacer->width();
    w += s;
    return statusBar()->width() - w - 30;
}

QString MW::getPosition()
{
/*
    This function is used by MW::updateStatus to report the current image relative to all
    the images in the folder ie 17 of 80.

    It is displayed on the status bar and in the infoView.
*/
    if (G::isLogger) G::log("MW::getPosition");
    QString fileCount = "";
    // QModelIndex idx = thumbView->currentIndex();
    // if (!idx.isValid()) return "";
    long rowCount = dm->sf->rowCount();
    /*  WHILE A STREAMED FILL IS RUNNING, report what the set WILL hold rather than what
        has arrived. A catalog scope of 43,000 images says "1 of 43,000" from the first
        thumbnail instead of counting up as batches land -- the chunking is the loader's
        business. Only while loading, and only when the proxy is showing everything the
        model holds: once a filter is applied the proxy's own count is the honest answer
        to "of how many", and the set's eventual size is not. */
    if (dm->loadingModel && dm->expectedRows > rowCount
        && dm->rowCount() == dm->sf->rowCount())
        rowCount = dm->expectedRows;
    if (rowCount <= 0) return "";
    int row = dm->currentSfRow + 1;
    fileCount = QString::number(row) + " of "
        + QString::number(rowCount);
    // if (subFoldersAction->isChecked()) fileCount += " including subfolders";
    return fileCount;
}

QString MW::getZoom()
{
/*

*/
    if (G::isLogger)
        G::log("MW::getZoom");
    if (G::mode != "Loupe" &&
        G::mode != "Compare") return "N/A";
    qreal zoom;
    if (G::mode == "Compare") zoom = compareImages->zoomValue;
    else zoom = imageView->zoom;
    // if (zoom <= 0 || zoom > 10) return "";
    // qDebug() << "MW::getZoom  zoom =" << zoom;
    return QString::number(qRound(zoom*100)) + "%"; // + "% zoom";
}

QString MW::getPicked()
{
/*
    Returns a string like "16 (38MB)"
*/
    if (G::isLogger) G::log("MW::getPicked");
    int count = 0;
    for (int row = 0; row < dm->sf->rowCount(); row++) {
        if (G::stop) return "";
        if (dm->valueSf(row, G::PickColumn) == "Picked") count++;   // dm->valueSF is thread safe
    }

    QString image = count == 1 ? " image, " : " images, ";

    if (count == 0) return "Nothing";
    return QString::number(count) + " ("  + pickMemSize + ")";
}

QString MW::getRawRendered()
{
    if (G::isLogger) G::log("MW::getRawRendered");
    if (dm->sf->index(dm->currentSfRow, G::RawRenderColumn).data().toBool()) {
        return "RAW";
    }
    return "";
}

QString MW::getSelectedFileSize()
{
/*
    Returns a string like "12 (165 MB)"
*/
    if (G::isLogger) G::log("MW::getSelectedFileSize");
    QString selected = QString::number(dm->selectionModel->selectedRows().count());
    QString selMemSize = Utilities::formatMemory(memoryReqdForSelection());
    return selected + " (" + selMemSize + ")";
}

void MW::updateProgressBarWidth()
{
    if (G::isLogger) G::log("MW::updateProgressBarWidth");
    if (dm->rowCount() && progress->isVisible()) {
        int availableSpace = availableSpaceForProgressBar();
        if (availableSpace < cacheBarProgressWidth) cacheBarProgressWidth = availableSpace;
        progress->setContainerWidth(cacheBarProgressWidth);
        imageCache->updateStatus(ImageCache::StatusAction::All, "MW::updateProgressBarWidth");
    }
}

void MW::setCacheProgressEnabled(bool on)
{
    if (G::isLogger) G::log("MW::setCacheProgressEnabled");
    /* Gate the cache rows (ImageCache + MetaRead) as a group so they honor the
       "Show caching progress" preference while other rows (e.g. FocusStack) can
       still appear. */
    progress->setRowEnabled(progress->imageCacheRow(), on);
    progress->setRowEnabled(progressMetaReadRow, on);
}

void MW::setCacheRunningLightsWidth() {
    QFontMetrics fm(metadataThreadRunningLabel->font());
    #ifdef Q_OS_WIN
    int charWidth = fm.horizontalAdvance(QStringLiteral("◉")) * 2;
    #endif
    #ifdef Q_OS_MAC
    int charWidth = fm.horizontalAdvance(QStringLiteral("◉")) * 1.3;
    #endif
    metadataThreadRunningLabel->setFixedWidth(charWidth);
    imageThreadRunningLabel->setFixedWidth(charWidth);
}

QString MW::getImageCacheRunningTip(bool isAuto, quint64 maxMB)
{
    QString autoSize = QVariant(isAuto).toString();
    QString ceiling = QVariant(maxMB).toString();
    QString tip = "Image caching:\n";
    tip += "\n";
    tip += "  • Green:    \tAll cached\n";
    tip += "  • Red:      \tCaching in progress\n";
    tip += "  • Gray:     \tEmpty folder, no images to cache\n";
    tip += "\n";
    tip += "Auto size cache = " + autoSize + "\n";
    tip += "Cache ceiling   = " + ceiling + "MB\n";
    tip += "\n";
    tip += "Click to go to cache preferences.";
    return tip;
}

void MW::updateMetadataThreadRunStatus(bool isRunning, bool success)
{
    // if (G::instanceClash(instance, "MW::updateMetadataThreadRunStatus")) return;

    if (G::isLogger) G::log("MW::updateMetadataThreadRunStatus");
    /*
    qDebug() << "MW::updateMetadataThreadRunStatus"
             << "isRunning =" << isRunning
             << "success =" << success
             << "calledBy =" << src
                ;
             //*/

    if (isRunning) {
        metadataThreadRunningLabel->setStyleSheet(G::cssError);
    }
    else if (!dm->rowCount()) {
        metadataThreadRunningLabel->setStyleSheet(G::cssInactive);
    }
    else {
        if (success) {
            /* Icon-cache mode indicator: orange under forced pressure (test override),
               blue when JIT caching is active, green for brute force. */
            if (G::iconPressureTestLevel >= 2)
                metadataThreadRunningLabel->setStyleSheet("QLabel,QLineEdit,QComboBox {color:orange;}");
            else if (G::useJitIconCache)
                metadataThreadRunningLabel->setStyleSheet("QLabel,QLineEdit,QComboBox {color:blue;}");
            else
                metadataThreadRunningLabel->setStyleSheet(G::cssOk);
        }
        else {
            metadataThreadRunningLabel->setStyleSheet(G::cssWarning);
        }
    }
    metadataThreadRunningLabel->setText("◉");
}

void MW::updateImageCachingThreadRunStatus(bool isRunning, bool showCacheLabel)
{
    if (G::isLogger) G::log("MW::updateImageCachingThreadRunStatus");
    if (isRunning) {
        if (G::isTest) testTime.restart();
        imageThreadRunningLabel->setStyleSheet(G::cssError);
    }
    else {
        if (G::isTest) {
            int ms = testTime.elapsed();
            int n = icd->imCache.count();
            if (n)
            qDebug() << "MW::updateImageCachingThreadRunStatus"
                    << "Total time to fill cache =" << ms
                    << "   Images cached =" << n
                    << "   ms per image =" << ms / n
                       ;
        }
        imageThreadRunningLabel->setStyleSheet(G::cssOk);
    }
    imageThreadRunningLabel->setText("◉");
    if (G::showCacheProgress) {
        /* Show the ImageCache row while caching is active; Progress shows/hides its
           own container from row content. */
        progress->showRow(progress->imageCacheRow(), showCacheLabel);
        bool isAutoSize = imageCache->getAutoMaxMB();
        quint64 maxMB = imageCache->getMaxMB();
        QString tip = getImageCacheRunningTip(isAutoSize, maxMB);
        imageThreadRunningLabel->setToolTip(tip);
    }
}

void MW::setThreadRunStatusInactive()
{
    if (G::isLogger) G::log("MW::setThreadRunStatusInactive");
    metadataThreadRunningLabel->setStyleSheet(G::cssInactive);
    imageThreadRunningLabel->setStyleSheet(G::cssInactive);
    metadataThreadRunningLabel->setText("◉");
    imageThreadRunningLabel->setText("◉");
}

void MW::resortImageCache()
{
    // if (G::isLogger)
        G::log("MW::resortImageCache");
    if (!dm->sf->rowCount()) return;
    QString currentFilePath = dm->currentDmIdx.data(G::PathRole).toString();
    emit imageCacheFilterChange(currentFilePath, "MW::resortImageCache");
    // change to ImageCache
    emit setImageCachePosition(dm->currentFilePath, "MW::resortImageCache");
//    emit setImageCachePosition(dm->currentFilePath);
}

void MW::enableStatusBarBtns()
{
    if (G::isLogger) G::log("MW::enableStatusBarBtns");
    bool enable = dm->rowCount() > 0 && G::allMetadataAttempted;
    // colorManageToggleBtn->setEnabled(enable);
    // reverseSortBtn->setEnabled(enable);
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

    sortReverseAction->setChecked(isReverseSort);
    updateStatusBar();  // updates btn image and tooltip
}

void MW::sortIndicatorChanged(int column, Qt::SortOrder sortOrder)
{
/*
    This slot function is triggered by the tableView->horizontalHeader sortIndicatorChanged
    signal being emitted, when a column header is clicked.  The menu checked status is
    updated to match the column sorted.  However, changing the menu checked state for any
    of the menu sort actions triggers a sort, which needs to be suppressed while syncing
    the menu actions with tableView.

    THE PROXY HAS NOT SORTED YET WHEN THIS ARRIVES.  QTableView does the sorting from its
    own slot on the same signal, and MW::reset toggles setSortingEnabled off and on, which
    RE-REGISTERS that slot behind this one -- so from the first folder load onwards this
    function runs FIRST, before the sort.  Everything that follows a sort was therefore
    computed against the OLD order: dm->currentSfRow (and the delegates' currentRow, the
    scroll and the image cache resort) kept the pre-sort row while the selection model,
    which Qt remaps correctly across the sort, moved to the new one.  The current image
    appeared to jump, which the menu sort never did.

    So the tail runs QUEUED -- after QTableView's slot, whatever order the connections are
    in -- and it runs through sortChange, the SAME function the sort menu uses, rather
    than a second copy of the same steps.  Re-sorting there is a no-op:
    QSortFilterProxyModel::sort early-returns on the column and order it already holds.
*/
    // if (G::isLogger)
        G::log("MW::sortIndicatorChanged");
//    QString columnName = tableView->model()->headerData(column, Qt::Horizontal).toString();
//    qDebug() << "MW::sortIndicatorChanged" << column << columnName << sortOrder << sortColumn;
    if (!G::allMetadataAttempted && column > G::DimensionsColumn) loadEntireMetadataCache("SortChange");

    sortMenuUpdateToMatchTable = true; // suppress sorting to update menu
    switch (column) {
    case G::NameColumn: sortFileNameAction->setChecked(true); break;
    case G::PickColumn: sortPickAction->setChecked(true); break;
    case G::LabelColumn: sortLabelAction->setChecked(true); break;
    case G::RatingColumn: sortRatingAction->setChecked(true); break;
    case G::TypeColumn: sortFileTypeAction->setChecked(true); break;
    case G::ByteSizeColumn: sortFileSizeAction->setChecked(true); break;
    case G::CreatedColumn: sortCreateAction->setChecked(true); break;
    case G::ModifiedColumn: sortModifyAction->setChecked(true); break;
    case G::CreatorColumn: sortCreatorAction->setChecked(true); break;
    case G::MegaPixelsColumn: sortMegaPixelsAction->setChecked(true); break;
    case G::DimensionsColumn: sortDimensionsAction->setChecked(true); break;
    case G::ApertureColumn: sortApertureAction->setChecked(true); break;
    case G::ShutterspeedColumn: sortShutterSpeedAction->setChecked(true); break;
    case G::ISOColumn: sortISOAction->setChecked(true); break;
    case G::CameraModelColumn: sortModelAction->setChecked(true); break;
    case G::LensColumn: sortLensAction->setChecked(true); break;
    case G::FocalLengthColumn: sortFocalLengthAction->setChecked(true); break;
    case G::TitleColumn: sortTitleAction->setChecked(true); break;
    default:
        // table column clicked and sorted that is not a menu sort item - uncheck all menu items
        if (sortFileNameAction->isChecked()) sortFileNameAction->setChecked(false);
        if (sortFileTypeAction->isChecked()) sortFileTypeAction->setChecked(false);
        if (sortFileSizeAction->isChecked()) sortFileSizeAction->setChecked(false);
        if (sortCreateAction->isChecked()) sortCreateAction->setChecked(false);
        if (sortModifyAction->isChecked()) sortModifyAction->setChecked(false);
        if (sortPickAction->isChecked()) sortPickAction->setChecked(false);
        if (sortLabelAction->isChecked()) sortLabelAction->setChecked(false);
        if (sortRatingAction->isChecked()) sortRatingAction->setChecked(false);
        if (sortMegaPixelsAction->isChecked()) sortMegaPixelsAction->setChecked(false);
        if (sortDimensionsAction->isChecked()) sortDimensionsAction->setChecked(false);
        if (sortApertureAction->isChecked()) sortApertureAction->setChecked(false);
        if (sortShutterSpeedAction->isChecked()) sortShutterSpeedAction->setChecked(false);
        if (sortISOAction->isChecked()) sortISOAction->setChecked(false);
        if (sortModelAction->isChecked()) sortModelAction->setChecked(false);
        if (sortFocalLengthAction->isChecked()) sortFocalLengthAction->setChecked(false);
        if (sortTitleAction->isChecked()) sortTitleAction->setChecked(false);
    }
    sortMenuUpdateToMatchTable = false;

    /* Sync MW's own idea of the sort with the table.  Checking a menu action does not
       emit triggered, so sortChangeFromAction never runs and sortColumn/isReverseSort
       used to stay on whatever the menu last sorted by -- which MW::filterChange reads
       (dm->sf->sortColumn() == sortColumn) to decide whether the sort needs recovering,
       and would answer no, then re-sort back to the stale column on the next filter
       change. toggleSortDirection also sets sortReverseAction and the statusbar button. */
    sortColumn = column;
    toggleSortDirection(sortOrder == Qt::DescendingOrder ? Tog::on : Tog::off);

    // the rest, after QTableView has actually sorted the proxy (see above)
    QTimer::singleShot(0, this, [this]{ sortChange("MW::sortIndicatorChanged"); });
}

void MW::toggleModifyImagesClick()
{
/*
    This is called by connect signals from the menu action and the color manage button.  The
    call is redirected to toggleColorManage, which has a parameter which is not supported
    by the action and button signals.
*/
    if (G::isLogger) G::log("MW::toggleModifyImagesClick");
    G::modifySourceFiles = !G::modifySourceFiles;
    toggleModifyImages();
    if (preferencesDlg == nullptr) return;
    pref->setItemValue("modifySourceFiles", G::modifySourceFiles);
}

void MW::toggleModifyImages()
{
    if (G::isLogger) G::log("MW::setModifyImages");

    // No menu item / action to toggle
    updateStatusBar();  // updates btn image and tooltip
}

void MW::toggleIncludeSidecarsClick()
{
/*
    This is called by connect signals from the menu action and the include sidecars
    button. The call is redirected to toggleColorManage, which has a parameter which is
    not supported by the action and button signals.
*/
    if (G::isLogger) G::log("MW::toggleIncludeSidecarsClick");
    toggleIncludeSidecars(Tog::toggle);
}

void MW::toggleIncludeSidecars(Tog n)
{
/*
    The image cache is rebuilt to show the color manage option.
*/
    if (G::isLogger) G::log("MW::toggleColorManage");
    if (n == Tog::toggle) G::includeSidecars = !G::includeSidecars;
    if (n == Tog::off) G::includeSidecars = false;
    if (n == Tog::on) G::includeSidecars = true;
    includeSidecarsAction->setChecked(G::includeSidecars);

    updateStatusBar();  // updates btn image and tooltip
}

void MW::toggleColorManageClick()
{
    /*
    This is called by connect signals from the menu action and the color manage button.  The
    call is redirected to toggleColorManage, which has a parameter which is not supported
    by the action and button signals.
*/
    if (G::isLogger) G::log("MW::toggleColorManageClick");
    toggleColorManage(Tog::toggle);
}

void MW::toggleColorManage(Tog n)
{
    /*
    The image cache is rebuilt to show the color manage option.
*/
    if (G::isLogger) G::log("MW::toggleColorManage");
    if (n == Tog::toggle) G::colorManage = !G::colorManage;
    if (n == Tog::off) G::colorManage = false;
    if (n == Tog::on) G::colorManage = true;
    colorManageAction->setChecked(G::colorManage);

    updateStatusBar();  // updates btn image and tooltip

    if (dm->rowCount() == 0) return;

    // set the isCached indicator on thumbnails to false (shows red dot on bottom right)
    for (int row = 0; row < dm->rowCount(); ++row) {
        QString fPath = dm->index(row, G::PathColumn).data(G::PathRole).toString();
        refreshViewsOnCacheChange(fPath, false, "MW::toggleColorManage");
    }

    // // let ImageView know that the image changed
    imageView->currentImageHasChanged = true;

    // reload image cache
    emit imageCacheColorManageChange();
}

void MW::refreshDevelopThumbs()
{
/*
    Re-read the thumbnail of every image that has a devPreview, because which picture that
    thumbnail should show has just changed.

    THE EFFECTIVE SOURCE IS NOT G::previewSource ALONE -- it is

        (G::operationMode == Develop) ? Developed : G::previewSource

    (the test Thumb::loadDevThumb makes). So this has to run on a MODE change as well as on
    a picture change: entering Develop while set to Original must replace the camera thumbs
    with developed ones, and leaving it must put them back. Callers decide when the
    effective source actually moved; this function just does the work.

    Only rows that carry a devPreview key are touched. The rest have one picture, so
    re-reading them would cost a folder's worth of I/O to produce identical pixels.
*/
    if (G::isLogger) G::log("MW::refreshDevelopThumbs");
    if (!dm || dm->rowCount() == 0) return;

    /* Drop the icons and re-read them. The developed thumbnail OVERWRITES the row's icon
       (DataModel::setDevelopIcon) -- the camera thumb is not kept alongside it -- so
       getting either one back means re-reading it. */
    bool anyCleared = false;
    for (int row = 0; row < dm->rowCount(); ++row) {
        if (!dm->index(row, G::DevPreviewKeyColumn).data().toString().isEmpty()) {
            dm->clearDevelopIcon(row);
            anyCleared = true;
        }
    }
    if (!anyCleared) return;            // nothing in this folder is developed

    /* The delegates keep their own scaled QPixmap per row, so they have to be told too or
       they keep painting the old thumbnail. */
    if (thumbView && thumbView->iconViewDelegate)
        thumbView->iconViewDelegate->clearAllCache();
    if (gridView && gridView->iconViewDelegate)
        gridView->iconViewDelegate->clearAllCache();

    /* MetaRead remembers every row a reader has already returned for this folder and
       refuses to read it again (readSuccessThisCycle / needToRead). It re-arms rows whose
       icon is missing only when the icon chunk is a WINDOW -- in the default brute-force
       mode it assumes a loaded icon stays loaded, which was true until previews started
       discarding them. Tell it otherwise, or the cleared thumbnails are never re-read and
       simply disappear. Queued ahead of reloadIconChunk's own queued call, so it is
       observed by the setStartRow that has to act on it. */
    QMetaObject::invokeMethod(metaRead, "invalidateLoadedIcons", Qt::QueuedConnection);
    reloadIconChunk();
}

void MW::syncPreviewSourceEnabled()
{
/*
    Original / Developed is a Preview-mode choice only. DEVELOP MODE ALWAYS SHOWS THE
    DEVELOPED IMAGE -- in the loupe, which is a live render of the recipe being edited, and
    in the thumbnails, which must agree with it. Offering the choice there would let the
    user ask for a picture Develop is not able to show.

    So in Develop the combo is DISABLED and reads Developed, and the two menu items and Y
    are disabled with a reason (a disabled QAction greys its menu item AND stops its
    shortcut firing, so one gate covers both -- the reason surfaces through the Disabled
    Shortcut Feedback path).

    THE PREFERENCE ITSELF IS NOT CHANGED. G::previewSource keeps whatever the user chose
    for Preview mode, and the combo shows Developed as a statement about the mode rather
    than as a new setting. Entering Develop must not silently rewrite a Preview-mode
    preference the user will get back the moment they leave.
*/
    if (G::isLogger) G::log("MW::syncPreviewSourceEnabled");
    const bool inDevelop = G::operationMode == G::OperationMode::Develop;
    const QString reason = "Develop mode always shows the developed image";

    if (previewSourceCombo) {
        QSignalBlocker block(previewSourceCombo);
        previewSourceCombo->setEnabled(!inDevelop);
        previewSourceCombo->setCurrentIndex(inDevelop ? int(G::PreviewSource::Developed)
                                                      : int(G::previewSource));
        previewSourceCombo->setToolTip(
            inDevelop
                ? QString("Develop mode always shows the developed image.\n"
                          "Leave Develop (E, G or T) to see the original.")
                : QString("What you are looking at:\n"
                          "  Original \u2014 the camera's picture, as shot\n"
                          "  Developed \u2014 your develop edits applied\n"
                          "Shortcut: Y"));
    }

    for (QAction *a : {previewSourceOriginalAction, previewSourceDevelopedAction,
                       togglePreviewSourceAction}) {
        if (!a) continue;
        a->setEnabled(!inDevelop);
        a->setProperty("disabledReason", reason);
    }
    /* The radio pair still reports the stored choice, so returning to Preview shows the
       picture the user last asked for rather than whatever Develop implied. */
    if (previewSourceOriginalAction)
        previewSourceOriginalAction->setChecked(G::previewSource == G::PreviewSource::Original);
    if (previewSourceDevelopedAction)
        previewSourceDevelopedAction->setChecked(G::previewSource == G::PreviewSource::Developed);
}

void MW::togglePreviewSource()
{
/*
    Flip between the camera's picture and the developed one.

    Unreachable in Develop mode: syncPreviewSourceEnabled disables the action there, and a
    disabled QAction's shortcut does not fire. The guard below is belt and braces for a
    direct call.
*/
    if (G::isLogger) G::log("MW::togglePreviewSource");
    if (G::operationMode == G::OperationMode::Develop) return;
    setPreviewSource(G::previewSource == G::PreviewSource::Developed
                         ? G::PreviewSource::Original
                         : G::PreviewSource::Developed);
}

void MW::setPreviewSource(G::PreviewSource source)
{
/*
    Choose which of an image's two pictures the grid and the loupe show outside Develop
    mode: the camera's (Original) or the render of its develop recipe (Developed).

    THIS CHANGES WHAT A CACHED DECODE MEANS, exactly as toggleUseRaw does, so both caches
    have to be rebuilt rather than merely repainted:

      - the icons, because Thumb::loadDevThumb is gated on this and the developed thumbnail
        OVERWRITES the row's icon (DataModel::setDevelopIcon) -- the camera thumb is not
        kept alongside it, so getting it back means re-reading it. reloadIconChunk
        re-dispatches MetaRead, which loads the visible rows first.
      - the full-size image cache, because ImageDecoder::loadDevPreview consults this flag
        to decide whether to serve the devPreview or decode the file.

    Only images that HAVE a devPreview change appearance; the rest re-read to the same
    pixels. That is wasted work for an unedited folder, which is why this is a deliberate
    user action and not something that fires on selection.
*/
    if (G::isLogger) G::log("MW::setPreviewSource");
    if (G::previewSource == source) return;
    G::previewSource = source;

    syncPreviewSourceEnabled();     // combo row + the radio pair

    settings->setValue("previewSource", static_cast<int>(G::previewSource));

    if (dm->rowCount() == 0) return;

    refreshDevelopThumbs();

    // set the isCached indicator on thumbnails to false (shows red dot on bottom right)
    for (int row = 0; row < dm->rowCount(); ++row) {
        QString fPath = dm->index(row, G::PathColumn).data(G::PathRole).toString();
        refreshViewsOnCacheChange(fPath, false, "MW::setPreviewSource");
    }

    // let ImageView know that the image changed
    imageView->currentImageHasChanged = true;

    // reload image cache
    emit imageCacheColorManageChange();
}

void MW::toggleUseRawClick()
{
/*
    Called by the "Decode Raw" status bar button. Redirected to toggleUseRaw, which takes a
    Tog parameter not supported by the button's clicked() signal.
*/
    if (G::isLogger) G::log("MW::toggleUseRawClick");
    toggleUseRaw(Tog::toggle);
}

void MW::toggleUseRaw(Tog n)
{
/*
    When G::useRaw is on, ImageDecoder demosaics the raw sensor data; when off it shows the
    embedded preview/jpg. The full-size image cache is rebuilt so the change is visible.
*/
    if (G::isLogger) G::log("MW::toggleUseRaw");
    if (n == Tog::toggle) G::useRaw = !G::useRaw;
    if (n == Tog::off) G::useRaw = false;
    if (n == Tog::on) G::useRaw = true;

    updateStatusBar();  // updates btn image and tooltip

    /* Keep the Develop panel's "Edit: Raw / Embedded Preview" selector in sync with the status
       bar button (both drive G::useRaw). */
    if (developProperties) developProperties->syncEditRaw(G::useRaw);

    if (dm->rowCount() == 0) return;

    // set the isCached indicator on thumbnails to false (shows red dot on bottom right)
    for (int row = 0; row < dm->rowCount(); ++row) {
        QString fPath = dm->index(row, G::PathColumn).data(G::PathRole).toString();
        refreshViewsOnCacheChange(fPath, false, "MW::toggleUseRaw");
    }

    // let ImageView know that the image changed
    imageView->currentImageHasChanged = true;

    // reload image cache
    emit imageCacheColorManageChange();
}

void MW::togglePanToFocusClick()
{
/*
    This is called by connect signals from the menu action and the PanToFocus button.
*/
    if (G::isLogger)
        G::log("MW::togglePanToFocusClick");
    togglePanToFocus(Tog::toggle);
}

void MW::togglePanToFocus(Tog n)
{
    if (G::isLogger)
        G::log("MW::togglePanToFocus");
    if (n == Tog::toggle) imageView->panToFocus = !imageView->panToFocus;
    if (n == Tog::off) imageView->panToFocus = false;
    if (n == Tog::on) imageView->panToFocus = true;

    // Hijack to create a training collection of thumbnails
    G::isTraining = imageView->panToFocus;

    updateStatusBar();  // updates btn image and tooltip

    if (imageView->panToFocus) {
        if (!imageView->pmItem->pixmap().isNull()) {
            imageView->predictPanToFocus();
            imageView->showNormalizedViewport(true, true, "MW::togglePanToFocus");
        }
    }
}

void MW::showPopUp(QString msg, int duration, bool isAutosize,
                   float opacity, Qt::Alignment alignment)
{
    G::popup->showPopup(msg, duration, isAutosize, opacity, alignment);
}
