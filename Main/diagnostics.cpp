#include "Main/mainwindow.h"

void MW::reportState()
{
    if (G::isLogger) G::log(__FUNCTION__);
    workspaceData w;
    snapshotWorkspace(w);
    qDebug() << G::t.restart() << "\t" << "\nisMaximized" << w.isFullScreen
             << "\nisWindowTitleBarVisible" << w.isWindowTitleBarVisible
             << "\nisMenuBarVisible" << w.isMenuBarVisible
             << "\nisStatusBarVisible" << w.isStatusBarVisible
             << "\nisFolderDockVisible" << w.isFolderDockVisible
             << "\nisFavDockVisible" << w.isFavDockVisible
             << "\nisFilterDockVisible" << w.isFilterDockVisible
             << "\nisMetadataDockVisible" << w.isMetadataDockVisible
             << "\nisEmbelDockVisible" << w.isEmbelDockVisible
             << "\nisThumbDockVisible" << w.isThumbDockVisible
             << "\nthumbSpacing" << w.thumbSpacing
             << "\nthumbPadding" << w.thumbPadding
             << "\nthumbWidth" << w.thumbWidth
             << "\nthumbHeight" << w.thumbHeight
             << "\nlabelFontSize" << w.labelFontSize
             << "\nshowThumbLabels" << w.showThumbLabels
             << "\nthumbSpacingGrid" << w.thumbSpacingGrid
             << "\nthumbPaddingGrid" << w.thumbPaddingGrid
             << "\nthumbWidthGrid" << w.thumbWidthGrid
             << "\nthumbHeightGrid" << w.thumbHeightGrid
             << "\nlabelFontSizeGrid" << w.labelFontSizeGrid
             << "\nshowThumbLabelsGrid" << w.showThumbLabelsGrid
             << "\nshowShootingInfo" << w.isImageInfoVisible
             << "\nisLoupeDisplay" << w.isLoupeDisplay
             << "\nisGridDisplay" << w.isGridDisplay
             << "\nisTableDisplay" << w.isTableDisplay
             << "\nisCompareDisplay" << w.isCompareDisplay
             << "\nisEmbelDisplay" << w.isEmbelDisplay
             << "\nisColorManage" << ws.isColorManage
             << "\ncacheSizeMethod" << ws.cacheSizeMethod
                ;
}

void MW::reportMetadata()
{
    if (G::isLogger) G::log(__FUNCTION__);
    diagnosticsMetadata();
}

// Diagnostic Reports

//QString MW::d(QVariant x)
//// helper function to convert variable values to a string for diagnostic reporting
//{
//    return QVariant(x).toString();
//}

void MW::diagnosticsAll()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << this->diagnostics();
    rpt << gridView->diagnostics();
    rpt << thumbView->diagnostics();
    rpt << imageView->diagnostics();
    rpt << embelProperties->diagnostics();
    rpt << metadata->diagnostics(dm->currentFilePath);
    rpt << dm->diagnostics();
    diagnosticsReport(reportString);
}

void MW::diagnosticsCurrent()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << dm->diagnosticsForCurrentRow();
    rpt << metadata->diagnostics(dm->currentFilePath);
    diagnosticsReport(reportString);
}

QString MW::diagnostics()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "MainWindow Diagnostics");
    rpt << "\n";

    rpt << "\n" << "version = " << G::s(version);
    rpt << "\n" << "isShift = " << G::s(isShift);
    rpt << "\n" << "ignoreSelectionChange = " << G::s(ignoreSelectionChange);
    rpt << "\n" << "lastPrefPage = " << G::s(lastPrefPage);
//    rpt << "\n" << "mouseClickScroll = " << G::s(mouseClickScroll);
    rpt << "\n" << "displayPhysicalHorizontalPixels = " << G::s(G::displayPhysicalHorizontalPixels);
    rpt << "\n" << "displayPhysicalVerticalPixels = " << G::s(G::displayPhysicalVerticalPixels);
    rpt << "\n" << "checkIfUpdate = " << G::s(checkIfUpdate);
    rpt << "\n" << "isRatingBadgeVisible = " << G::s(isRatingBadgeVisible);
    rpt << "\n" << "classificationBadgeInImageDiameter = " << G::s(classificationBadgeInImageDiameter);
    rpt << "\n" << "classificationBadgeInThumbDiameter = " << G::s(classificationBadgeInThumbDiameter);
    rpt << "\n" << "rememberLastDir = " << G::s(rememberLastDir);
    rpt << "\n" << "lastDir = " << G::s(lastDir);
    rpt << "\n" << "ingestRootFolder = " << G::s(ingestRootFolder);
    rpt << "\n" << "ingestRootFolder2 = " << G::s(ingestRootFolder2);
    rpt << "\n" << "pathTemplateSelected = " << G::s(pathTemplateSelected);
    rpt << "\n" << "pathTemplateSelected2 = " << G::s(pathTemplateSelected2);
    rpt << "\n" << "filenameTemplateSelected = " << G::s(filenameTemplateSelected);
    rpt << "\n" << "manualFolderPath = " << G::s(manualFolderPath);
    rpt << "\n" << "manualFolderPath2 = " << G::s(manualFolderPath2);
    rpt << "\n" << "combineRawJpg = " << G::s(combineRawJpg);
    rpt << "\n" << "autoIngestFolderPath = " << G::s(autoIngestFolderPath);
    rpt << "\n" << "autoEjectUsb = " << G::s(autoEjectUsb);
    rpt << "\n" << "integrityCheck = " << G::s(integrityCheck);
    rpt << "\n" << "isBackgroundIngest = " << G::s(isBackgroundIngest);
    rpt << "\n" << "isBackgroundIngestBeep = " << G::s(isBackgroundIngestBeep);
    rpt << "\n" << "ingestIncludeXmpSidecar = " << G::s(ingestIncludeXmpSidecar);
    rpt << "\n" << "backupIngest = " << G::s(backupIngest);
    rpt << "\n" << "gotoIngestFolder = " << G::s(gotoIngestFolder);
    rpt << "\n" << "lastIngestLocation = " << G::s(lastIngestLocation);
    rpt << "\n" << "slideShowDelay = " << G::s(slideShowDelay);
    rpt << "\n" << "slideShowRandom = " << G::s(isSlideShowRandom);
    rpt << "\n" << "slideShowWrap = " << G::s(isSlideShowWrap);
    rpt << "\n" << "cacheSizeMB = " << G::s(cacheMaxMB);
    rpt << "\n" << "showCacheStatus = " << G::s(isShowCacheProgressBar);
    rpt << "\n" << "cacheDelay = " << G::s(cacheDelay);
    rpt << "\n" << "isShowCacheThreadActivity = " << G::s(isShowCacheProgressBar);
    rpt << "\n" << "progressWidth = " << G::s(progressWidth);
    rpt << "\n" << "cacheWtAhead = " << G::s(cacheWtAhead);
    rpt << "\n" << "isCachePreview = " << G::s(isCachePreview);
    rpt << "\n" << "cachePreviewWidth = " << G::s(cachePreviewWidth);
    rpt << "\n" << "cachePreviewHeight = " << G::s(cachePreviewHeight);
    rpt << "\n" << "fullScreenDocks.isFolders = " << G::s(fullScreenDocks.isFolders);
    rpt << "\n" << "fullScreenDocks.isFavs = " << G::s(fullScreenDocks.isFavs);
    rpt << "\n" << "fullScreenDocks.isFilters = " << G::s(fullScreenDocks.isFilters);
    rpt << "\n" << "fullScreenDocks.isMetadata = " << G::s(fullScreenDocks.isMetadata);
    rpt << "\n" << "fullScreenDocks.isThumbs = " << G::s(fullScreenDocks.isThumbs);
    rpt << "\n" << "fullScreenDocks.isStatusBar = " << G::s(fullScreenDocks.isStatusBar);
    rpt << "\n" << "isNormalScreen = " << G::s(isNormalScreen);
    rpt << "\n" << "currentViewDir = " << G::s(currentViewDirPath);
    rpt << "\n" << "prevMode = " << G::s(prevMode);
    rpt << "\n" << "currentRow = " << G::s(currentRow);
    rpt << "\n" << "scrollRow = " << G::s(scrollRow);
    rpt << "\n" << "currentDmIdx = row" << G::s(currentDmIdx.row()) << " col " << G::s(currentDmIdx.column());
    rpt << "\n" << "allIconsLoaded = " << G::s(allIconsLoaded);
    rpt << "\n" << "modeChangeJustHappened = " << G::s(modeChangeJustHappened);
    rpt << "\n" << "justUpdatedBestFit = " << G::s(justUpdatedBestFit);
    rpt << "\n" << "sortColumn = " << G::s(sortColumn);
    rpt << "\n" << "showImageCount = " << G::s(showImageCount);
    rpt << "\n" << "isCurrentFolderOkay = " << G::s(isCurrentFolderOkay);
    rpt << "\n" << "G::isSlideShow = " << G::s(G::isSlideShow);
    rpt << "\n" << "copyOp = " << G::s(copyOp);
    rpt << "\n" << "isDragDrop = " << G::s(isDragDrop);
    rpt << "\n" << "dragDropFilePath = " << G::s(dragDropFilePath);
    rpt << "\n" << "dragDropFolderPath = " << G::s(dragDropFolderPath);
    rpt << "\n" << "maxThumbSpaceHeight = " << G::s(maxThumbSpaceHeight);
    rpt << "\n" << "pickMemSize = " << G::s(pickMemSize);
    rpt << "\n" << "ignoreDockResize = " << G::s(ignoreDockResize);
    rpt << "\n" << "wasThumbDockVisible = " << G::s(wasThumbDockVisible);
    rpt << "\n" << "workspaceChange = " << G::s(workspaceChange);
    rpt << "\n" << "isUpdatingState = " << G::s(isUpdatingState);
    rpt << "\n" << "isFilterChange = " << G::s(isFilterChange);
    rpt << "\n" << "isRefreshingDM = " << G::s(isRefreshingDM);
    rpt << "\n" << "refreshCurrentPath = " << G::s(refreshCurrentPath);
    rpt << "\n" << "simulateJustInstalled = " << G::s(simulateJustInstalled);
    rpt << "\n" << "isSettings = " << G::s(isSettings);
    rpt << "\n" << "isStressTest = " << G::s(isStressTest);
    rpt << "\n" << "hasGridBeenActivated = " << G::s(hasGridBeenActivated);
    rpt << "\n" << "isLeftMouseBtnPressed = " << G::s(isLeftMouseBtnPressed);
    rpt << "\n" << "isMouseDrag = " << G::s(isMouseDrag);
//    rpt << "\n" << "timeToQuit = " << G::s(timeToQuit);
    rpt << "\n" << "sortMenuUpdateToMatchTable = " << G::s(sortMenuUpdateToMatchTable);
    rpt << "\n" << "imageCacheFilePath = " << G::s(imageCacheFilePath);
    rpt << "\n" << "newScrollSignal = " << G::s(newScrollSignal);
    rpt << "\n" << "prevCentralView = " << G::s(prevCentralView);
    rpt << "\n" << "mouseOverFolder = " << G::s(mouseOverFolder);
    rpt << "\n" << "rating = " << G::s(rating);
    rpt << "\n" << "colorClass = " << G::s(colorClass);
    rpt << "\n" << "isPick = " << G::s(isPick);

    rpt << "\n\n" ;
    return reportString;
}

void MW::diagnosticsMain() {diagnosticsReport(this->diagnostics());}
void MW::diagnosticsGridView() {diagnosticsReport(gridView->diagnostics());}
void MW::diagnosticsThumbView() {diagnosticsReport(thumbView->diagnostics());}
void MW::diagnosticsImageView() {diagnosticsReport(imageView->diagnostics());}
void MW::diagnosticsInfoView() {}
void MW::diagnosticsTableView() {}
void MW::diagnosticsCompareView() {}
void MW::diagnosticsMetadata()
{
    dm->imMetadata(dm->currentFilePath, true);
    diagnosticsReport(metadata->diagnostics(dm->currentFilePath));
}
void MW::diagnosticsXMP() {}
void MW::diagnosticsMetadataCache() {}
void MW::diagnosticsImageCache() {diagnosticsReport(imageCacheThread->diagnostics());}
void MW::diagnosticsDataModel() {diagnosticsReport(dm->diagnostics());}
void MW::diagnosticsErrors() {diagnosticsReport(dm->diagnosticsErrors());}
void MW::diagnosticsEmbellish() {diagnosticsReport(embelProperties->diagnostics());}
void MW::diagnosticsFilters() {}
void MW::diagnosticsFileTree() {}
void MW::diagnosticsBookmarks() {}
void MW::diagnosticsPixmap() {}
void MW::diagnosticsThumb() {}
void MW::diagnosticsIngest() {}
void MW::diagnosticsZoom() {}

void MW::diagnosticsReport(QString reportString)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    md.textBrowser->setStyleSheet(G::css);
    QFont courier("Courier", 12);
    md.textBrowser->setFont(courier);
    md.textBrowser->setText(reportString);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
//    md.textBrowser->setFixedWidth(1800);
//    md.textBrowser->setSizePolicy(1800, 800);
    QFontMetrics metrics(md.textBrowser->font());
    md.textBrowser->setTabStopDistance(3 * metrics.horizontalAdvance(' '));
//    dlg->setBaseSize(1800, 800);
    dlg->show();
//    std::cout << reportString.toStdString() << std::flush;
}

void MW::errorReport()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    dlg->setWindowTitle("Winnow Error Log");
    md.textBrowser->setStyleSheet(G::css);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    G::errlogFile.seek(0);
    QString errString(G::errlogFile.readAll());
//    qDebug() << __FUNCTION__ << G::errlogFile.isOpen() << errString;
    md.textBrowser->setText(errString);
    dlg->show();
}
