#include "Main/mainwindow.h"

void MW::updateStatus(bool keepBase, QString s, QString source)
{
/*
    Reports status information on the status bar and in InfoView.  If keepBase = true
    then ie "1 of 80   60% zoom   2.1 MB picked" is prepended to the status message s.
*/
    if (!G::useUpdateStatus) return;
    if (G::stop) return;

    if (G::isLogger) G::log("MW::updateStatus");

    // check if instance clash (old folder signal)
    QString fPath = thumbView->getCurrentFilePath();
    int row = dm->rowFromPath(fPath);
    if ((row == -1) && (dm->instance > -1) && (fPath != "")) {
        qWarning() << "WARNING" << "MW::updateStatus"
                   << fPath
                   << "not found.  Probable instance clash";
        return;
    }

    // check if null filter
    if (dm->sf->rowCount() == 0) {
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

    /* Possible status info

    QString fName = idx.data(Qt::EditRole).toString();
    QString fPath = idx.data(G::PathRole).toString();
    QString shootingInfo = metadata->getShootingInfo(fPath);
    QString err = metadata->getErr(fPath);
    QString magnify = "🔎🔍";
    QString fileSym = "📁📂📗🕎📷🎨👍";
    QString camera = "📈📌🔈📎🔗🔑🧾🛈";
    //    https://www.vertex42.com/ExcelTips/unicode-symbols.html
    QString sym = "⚡🌈🌆🌸🍁🍄🎁🎹💥💭🏃🏸💻🔆🔴🔵🔶🔷🔸🔹🔺🔻🖐🧲🛑⛬🎞🚫";
//        */

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
    else base = "";

    // image of total: fileCount
    if (keepBase && isCurrentFolderOkay) {
//        base += "Mem: " + mem + spacer;
        if (G::mode == "Loupe" || G::mode == "Compare" || G::mode == "Embel") {
            base += "Zoom: " + getZoom();
        }
        base += spacer + "Pos: " + getPosition();
        if (source != "nextSlide") {
            QString s = QString::number(thumbView->getSelectedCount());
            base += spacer +" Selected: " + s;
            base += spacer + "Picked: " + getPicked();
        }
    }

    status = " " + base + s;
    statusLabel->setText(status);
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

    if (G::colorManage) colorManageToggleBtn->setIcon(QIcon(":/images/icon16/rainbow1.png"));
    else colorManageToggleBtn->setIcon(QIcon(":/images/icon16/norainbow1.png"));

    if (sortReverseAction->isChecked()) reverseSortBtn->setIcon(QIcon(":/images/icon16/Z-A.png"));
    else reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));

    filterStatusLabel->setVisible(filters->isAnyFilter());
    subfolderStatusLabel->setVisible(subFoldersAction->isChecked());
    rawJpgStatusLabel->setVisible(combineRawJpgAction->isChecked());
    slideShowStatusLabel->setVisible(G::isSlideShow);

//    updateProgressBarWidth();

//    if (!(G::isSlideShow && isSlideShowRandom)) progressLabel->setVisible(isShowCacheProgressBar);// rghcachechange
//    else progressLabel->setVisible(false);
}

int MW::availableSpaceForProgressBar()
{
    if (G::isLogger) G::log("MW::availableSpaceForProgressBar");
    int w = 0;
    int s = statusBar()->layout()->spacing();
    if (colorManageToggleBtn->isVisible()) w += s + colorManageToggleBtn->width();
    if (reverseSortBtn->isVisible()) w += s + reverseSortBtn->width();
    if (rawJpgStatusLabel->isVisible()) w += s + rawJpgStatusLabel->width();
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
    This function is used by MW::updateStatus to report the current image relative to all the
    images in the folder ie 17 of 80.

    It is displayed on the status bar and in the infoView.
*/
    if (G::isLogger) G::log("MW::getPosition");
    QString fileCount = "";
    QModelIndex idx = thumbView->currentIndex();
    if (!idx.isValid()) return "";
    long rowCount = dm->sf->rowCount();
    if (rowCount <= 0) return "";
    int row = idx.row() + 1;
    fileCount = QString::number(row) + " of "
        + QString::number(rowCount);
    if (subFoldersAction->isChecked()) fileCount += " including subfolders";
    return fileCount;
}

QString MW::getZoom()
{
/*

*/
    if (G::isLogger) G::log("MW::getZoom");
    if (G::mode != "Loupe" &&
        G::mode != "Compare") return "N/A";
    qreal zoom;
    if (G::mode == "Compare") zoom = compareImages->zoomValue;
    else zoom = imageView->zoom;
    if (zoom <= 0 || zoom > 10) return "";
//    qDebug() << "MW::getZoom" << zoom;
    return QString::number(qRound(zoom*100)) + "%"; // + "% zoom";
}

QString MW::getPicked()
{
/*
    Returns a string like "16 (38MB)"
*/
    if (G::isLogger) G::log("MW::getPicked");
    int count = 0;
    for (int row = 0; row < dm->sf->rowCount(); row++)
        if (dm->sf->index(row, G::PickColumn).data() == "Picked") count++;
    QString image = count == 1 ? " image, " : " images, ";

    if (count == 0) return "Nothing";
    return QString::number(count) + " ("  + pickMemSize + ")";
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
    if (dm->rowCount() && progressLabel->isVisible()) {
        int availableSpace = availableSpaceForProgressBar();
        if (availableSpace < progressWidth) progressWidth = availableSpace;
        progressLabel->setFixedWidth(progressWidth);
        updateImageCacheStatus("Update all rows", icd->cache, "MW::updateProgressBarWidth");
    }
}

void MW::updateMetadataThreadRunStatus(bool isRunning, bool showCacheLabel, QString calledBy)
{
//    return;  //rghmacdelay
    if (G::isLogger) G::log("MW::updateMetadataThreadRunStatus");
//    qDebug() << "MW::updateMetadataThreadRunStatus" << isRunning;
    if (isRunning) {
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Red;}");
        #ifdef Q_OS_WIN
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Red;font-size: 24px;}");
        #endif
    }
    else {
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
        #ifdef Q_OS_WIN
        metadataThreadRunningLabel->setStyleSheet("QLabel {color:Green;font-size: 24px;}");
        #endif
    }
    metadataThreadRunningLabel->setText("◉");
    if (isShowCacheProgressBar && !G::isSlideShow) progressLabel->setVisible(showCacheLabel);
    calledBy = "";  // suppress compiler warning
}

void MW::updateImageCachingThreadRunStatus(bool isRunning, bool showCacheLabel)
{
//    return;  //rghmacdelay
    if (G::isLogger) G::log("MW::updateImageCachingThreadRunStatus");
    if (isRunning) {
        if (G::isTest) testTime.restart();
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Red;}");
        #ifdef Q_OS_WIN
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Red; font-size: 24px;}");
        #endif
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
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Green;}");
        #ifdef Q_OS_WIN
        imageThreadRunningLabel->setStyleSheet("QLabel {color:Green; font-size: 24px;}");
        #endif
    }
    imageThreadRunningLabel->setText("◉");
    if (isShowCacheProgressBar) progressLabel->setVisible(showCacheLabel);
}

void MW::setThreadRunStatusInactive()
{
    if (G::isLogger) G::log("MW::setThreadRunStatusInactive");
    metadataThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    imageThreadRunningLabel->setStyleSheet("QLabel {color:Gray;}");
    #ifdef Q_OS_WIN
    metadataThreadRunningLabel->setStyleSheet("QLabel {color:Gray;font-size: 24px;}");
    imageThreadRunningLabel->setStyleSheet("QLabel {color:Gray;font-size: 24px;}");
    #endif
    metadataThreadRunningLabel->setText("◉");
    imageThreadRunningLabel->setText("◉");
}

void MW::resortImageCache()
{
    if (G::isLogger) G::log("MW::resortImageCache");
    if (!dm->sf->rowCount()) return;
    QString currentFilePath = dm->currentDmIdx.data(G::PathRole).toString();
    imageCacheThread->rebuildImageCacheParameters(currentFilePath, "MW::resortImageCache");
    // change to ImageCache
    imageCacheThread->setCurrentPosition(dm->currentFilePath, "MW::resortImageCache");
//    emit setImageCachePosition(dm->currentFilePath);
}

void MW::sortIndicatorChanged(int column, Qt::SortOrder sortOrder)
{
/*
    This slot function is triggered by the tableView->horizontalHeader sortIndicatorChanged
    signal being emitted, which tells us that the data model sort/filter has been re-sorted.
    As a consequence we need to update the menu checked status for the correct column and also
    resync the image cache. However, changing the menu checked state for any of the menu sort
    actions triggers a sort, which needs to be suppressed while syncing the menu actions with
    tableView.
*/
    if (G::isLogger) G::log("MW::sortIndicatorChanged");
//    QString columnName = tableView->model()->headerData(column, Qt::Horizontal).toString();
//    qDebug() << "MW::sortIndicatorChanged" << column << columnName << sortOrder << sortColumn;
    if (!G::allMetadataLoaded && column > G::DimensionsColumn) loadEntireMetadataCache("SortChange");

    sortMenuUpdateToMatchTable = true; // suppress sorting to update menu
    switch (column) {
    case G::NameColumn: sortFileNameAction->setChecked(true); break;
    case G::PickColumn: sortPickAction->setChecked(true); break;
    case G::LabelColumn: sortLabelAction->setChecked(true); break;
    case G::RatingColumn: sortRatingAction->setChecked(true); break;
    case G::TypeColumn: sortFileTypeAction->setChecked(true); break;
    case G::SizeColumn: sortFileSizeAction->setChecked(true); break;
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
    if(sortOrder == Qt::DescendingOrder) sortReverseAction->setChecked(true);
    else sortReverseAction->setChecked(false);
    sortMenuUpdateToMatchTable = false;

    // get the current selected item
    dm->currentSfIdx = dm->sf->mapFromSource(dm->currentDmIdx);
    dm->currentSfRow = dm->currentSfIdx.row();
    thumbView->iconViewDelegate->currentRow = dm->currentSfRow;
    gridView->iconViewDelegate->currentRow = dm->currentSfRow;

    scrollToCurrentRow();

    resortImageCache();
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
    if (G::colorManage) {
        colorManageToggleBtn->setIcon(QIcon(":/images/icon16/rainbow1.png"));
    }
    else {
        colorManageToggleBtn->setIcon(QIcon(":/images/icon16/norainbow1.png"));
    }

    if (dm->rowCount() == 0) return;

//    imageView->loadImage(dm->currentFilePath, "MW::toggleColorManage", true/*refresh*/);

    // set the isCached indicator on thumbnails to false (shows red dot on bottom right)
    for (int row = 0; row < dm->rowCount(); ++row) {
        QString fPath = dm->index(row, G::PathColumn).data(G::PathRole).toString();
        updateCachedStatus(fPath, false, "MW::toggleColorManage");
    }
    // reload image cache
    imageCacheThread->colorManageChange();
}

void MW::toggleImageCacheMethod()
{
/*
    Called by cacheSizeBtn press
*/
    if (G::isLogger) G::log("MW::toggleImageCacheMethod");

    if (qApp->keyboardModifiers() == Qt::ControlModifier) {
        cachePreferences();
        return;
    }

    if (cacheSizeMethod == "Thrifty") setImageCacheSize("Moderate");
    else if (cacheSizeMethod == "Moderate") setImageCacheSize("Greedy");
    else if (cacheSizeMethod == "Greedy") setImageCacheSize("Thrifty");
    setImageCacheParameters();
}

