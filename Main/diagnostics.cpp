#include "Main/mainwindow.h"

void MW::fitDiagnostics(QDialog *dlg, QTextBrowser *textBrowser)
{
    if (G::isLogger) G::log("MW::fitDiagnostics");

    QFont courier("Courier", 12);
    textBrowser->setFont(courier);
    QFontMetrics metrics(textBrowser->font());
    textBrowser->setTabStopDistance(3 * metrics.horizontalAdvance(' '));
    QStringList lines = textBrowser->toPlainText().split('\n');
    int padding = 70;
    int maxWidth = 0;
    int maxHeight = 1200;

    // get widest line + add padding at end
    for (const QString &line : lines) {
        int width = metrics.horizontalAdvance(line);
        if (width > maxWidth) {
            maxWidth = width;
        }
    }
    maxWidth += padding;

    // Calculate total text height
    int lineHeight = metrics.height();
    int totalHeight = lineHeight * lines.size() + padding;

    int w, h;

    w = maxWidth;
    totalHeight < maxHeight ? h = totalHeight : h = 1200;
    // qDebug() << "MW::fitDiagnostics"
        // << w << h << G::displayPhysicalHorizontalPixels << G::displayPhysicalVerticalPixels;
    if (w > G::displayPhysicalHorizontalPixels) w = G::displayPhysicalHorizontalPixels - 100;
    if (h > G::displayPhysicalVerticalPixels) h = G::displayPhysicalVerticalPixels - 100;

    dlg->resize(w, h);
}

void MW::reportState(QString title)
{
    if (G::isLogger) G::log("MW::reportState");
    qDebug()
        << "\nWINNOW STATE" << title
        << "\nFLAGS:"
        << "\nG::isInitializing                      " << G::isInitializing
        << "\ndm->loadingModel                       " << dm->loadingModel
        << "\ndm->basicFileInfoLoaded                " << dm->basicFileInfoLoaded
        << "\nG::allMetadataLoaded                   " << G::allMetadataLoaded
        << "\nG::iconChunkLoaded                        " << G::iconChunkLoaded
        << "\nG::stop                                " << G::stop
        << "\nimageView->isFirstImageNewInstance     " << imageView->isFirstImageNewInstance

//        << "\nisDragDrop                             " << isDragDrop
//        << "\nG::ignoreScrollSignal                  " << G::ignoreScrollSignal
//        << "\nG::isRunningBackgroundIngest           " << G::isRunningBackgroundIngest
//        << "\nturnOffEmbellish                       " << turnOffEmbellish

        << "\nTHREADS:"
        << "\nImageCache   isRunning                 " << imageCache->isRunning()
        << "\nBuildFilters isRunning                 " << buildFilters->isRunning()

        << "\nPOSITION:"
        << "\nG::mode                                " << G::mode
        << "\ncentralLayout->currentIndex()          " << centralLayout->currentIndex()
        // rgh maybe just list all selected folders, or do we even care
        << "\nG::currRootFolder                      " << dm->folderList.at(0)
        << "\nccurrentFilePath                       " << dm->currentFilePath
        << "\ncurrentRow                             " << dm->currentSfRow
        << "\ncurrentSfIdx                           " << dm->currentSfIdx
        << "\ncurrentDmIdx                           " << dm->currentDmIdx
//        << "\ndm->firstVisibleRow                    " << dm->firstVisibleRow
//        << "\ndm->lastVisibleRow                     " << dm->lastVisibleRow
        << "\nG::availableMemoryMB                   " << G::availableMemoryMB
        << "\nG::winnowMemoryBeforeCacheMB           " << G::winnowMemoryBeforeCacheMB
        << "\nG::metaCacheMB                         " << G::metaCacheMB
        ;
}

void MW::reportWorkspaceState()
{
    if (G::isLogger) G::log("MW::reportWorkspaceState");
    WorkspaceData w;
    snapshotWorkspace(w);
    qDebug() << G::t.restart()
             << "\t" << "\nisMaximized" << w.isFullScreen
             << "\nisWindowTitleBarVisible" << w.isWindowTitleBarVisible
             //<< "\nisMenuBarVisible" << w.isMenuBarVisible
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
             // << "\nisEmbelDisplay" << w.isEmbelDisplay
             << "\nisColorManage" << ws.isColorManage
                ;
}

void MW::reportMetadata()
{
    if (G::isLogger) G::log("MW::reportMetadata");
    if (dm->rowCount()) diagnosticsMetadata();
    else G::popup->showPopup("No image selected");
}

// Diagnostic Reports

//QString MW::d(QVariant x)
//// helper function to convert variable values to a string for diagnostic reporting
//{
//    return QVariant(x).toString();
//}

void MW::diagnosticsAll()
{
    if (G::isLogger) G::log("MW::diagnosticsAll");
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
    if (G::isLogger) G::log("MW::diagnosticsCurrent");
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << dm->diagnosticsForCurrentRow();
    rpt << metadata->diagnostics(dm->currentFilePath);
    diagnosticsReport(reportString);
}

QString MW::diagnostics()
{
    if (G::isLogger) G::log("MW::diagnostics");
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "MainWindow Diagnostics");
    rpt << "\n";

    rpt << "\n" << "Winnow version = " << G::s(version);
    rpt << "\n" << "Winnow executable dir = " << G::s(qApp->applicationDirPath());
    rpt << "\n" << "Winnow executable path = " << G::s(qApp->applicationFilePath());
#ifdef Q_OS_WIN
    rpt << "\n";
    rpt << "\n" << "Preprocessor operating system";
    rpt << "\n" << "WINVER = " << G::s(WINVER);
    rpt << "\n" << "_WIN32_WINNT = " << G::s(_WIN32_WINNT);
    rpt << "\n" << "NTDDI_VERSION = " << G::s(NTDDI_VERSION);
#endif
    rpt << "\n";

    rpt << "\n" << "Settings iniPath = " << iniPath;
    rpt << "\n";

    rpt << "\n" << "G::winOutProfilePath = " << G::winOutProfilePath;
    rpt << "\n" << "G::strFontSize = " << G::strFontSize;
    rpt << "\n" << "G::displayPhysicalHorizontalPixels = " << G::s(G::displayPhysicalHorizontalPixels);
    rpt << "\n" << "G::displayPhysicalVerticalPixels = " << G::s(G::displayPhysicalVerticalPixels);
    rpt << "\n" << "G::displayVirtualHorizontalPixels = " << G::s(G::displayVirtualHorizontalPixels);
    rpt << "\n" << "G::displayVirtualVerticalPixels = " << G::s(G::displayVirtualVerticalPixels);
    rpt << "\n" << "G::actDevicePixelRatio = " << G::s(G::actDevicePixelRatio);
    rpt << "\n" << "G::sysDevicePixelRatio = " << G::s(G::sysDevicePixelRatio);
    rpt << "\n" << "G::fontSize = " << G::s(G::fontSize);
    rpt << "\n" << "G::ptToPx = " << G::ptToPx;
    rpt << "\n" << "G::dpi = " << G::dpi;

    rpt << "\n";

    rpt << "\n" << "G::isInitializing = " << G::s(G::isInitializing);
    rpt << "\n" << "G::stop = " << G::s(G::stop);
    rpt << "\n";
    rpt << "\n" << "G::allMetadataLoaded = " << G::s(G::allMetadataLoaded);
    rpt << "\n" << "G::iconChunkLoaded = " << G::s(G::iconChunkLoaded);
    rpt << "\n" << "dm->abortLoadingModel = " << G::s(dm->abort);
    rpt << "\n" << "dm->loadingModel = " << G::s(dm->loadingModel);
    rpt << "\n" << "dm->instance = " << G::s(dm->instance);
    rpt << "\n" << "G::dmInstance = " << G::s(G::dmInstance);
    rpt << "\n";

    rpt << "\n" << "G::isRory = " << G::s(G::isRory);
    rpt << "\n" << "G::useApplicationStateChanged = " << G::s(G::useApplicationStateChanged);
    rpt << "\n" << "G::useZoomWindow = " << G::s(G::useZoomWindow);
    rpt << "\n" << "G::useFSTreeCount = " << G::s(G::useFSTreeCount);
    rpt << "\n" << "G::useReadMeta = " << G::s(G::useReadMeta);
    rpt << "\n" << "G::useReadIcons = " << G::s(G::useReadIcons);
    rpt << "\n" << "G::useImageCache = " << G::s(G::useImageCache);
    rpt << "\n" << "G::useImageView = " << G::s(G::useImageView);
    rpt << "\n" << "G::useInfoView = " << G::s(G::useInfoView);
    rpt << "\n" << "G::useMultimedia = " << G::s(G::useMultimedia);
    rpt << "\n" << "G::useUpdateStatus = " << G::s(G::useUpdateStatus);
    rpt << "\n" << "G::useFilterView = " << G::s(G::useFilterView);
    rpt << "\n" << "G::useProcessEvents = " << G::s(G::useProcessEvents);
    rpt << "\n";
    rpt << "\n" << "G::strFontSize = " << G::s(G::strFontSize);
    rpt << "\n" << "G::fontSize = " << G::s(G::fontSize);
    rpt << "\n" << "G::dpi = " << G::s(G::dpi);
    rpt << "\n" << "G::ptToPx = " << G::s(G::ptToPx);
    rpt << "\n";
    rpt << "\n" << "G::labelNoneColor = " << G::s(G::labelNoneColor);
    rpt << "\n" << "G::labelRedColor = " << G::s(G::labelRedColor);
    rpt << "\n" << "G::labelYellowColor = " << G::s(G::labelYellowColor);
    rpt << "\n" << "G::labelGreenColor = " << G::s(G::labelGreenColor);
    rpt << "\n" << "G::labelBlueColor = " << G::s(G::labelBlueColor);
    rpt << "\n" << "G::labelPurpleColor = " << G::s(G::labelPurpleColor);
    rpt << "\n" << "G::labelColors = " << G::s(G::labelColors);
    rpt << "\n" << "G::ratings = " << G::s(G::ratings);
    rpt << "\n";
    rpt << "\n" << "G::scrollBarThickness = " << G::s(G::scrollBarThickness);
    rpt << "\n" << "G::propertyWidgetMarginLeft = " << G::s(G::propertyWidgetMarginLeft);
    rpt << "\n" << "G::propertyWidgetMarginRight = " << G::s(G::propertyWidgetMarginRight);
    rpt << "\n";
    rpt << "\n" << "G::iconOpacity = " << G::s(G::iconOpacity);
    rpt << "\n" << "G::wheelSensitivity = " << G::s(G::wheelSensitivity);
    rpt << "\n" << "G::wheelSpinning = " << G::s(G::wheelSpinning);
    rpt << "\n";
    rpt << "\n" << "G::loadOnlyVisibleIcons = " << G::s(G::loadOnlyVisibleIcons);
    rpt << "\n" << "G::availableMemoryMB = " << G::s(G::availableMemoryMB);
    rpt << "\n" << "G::winnowMemoryBeforeCacheMB = " << G::s(G::winnowMemoryBeforeCacheMB);
    rpt << "\n" << "G::metaCacheMB = " << G::s(G::metaCacheMB);
    rpt << "\n";
    rpt << "\n" << "G::mode = " << G::s(G::mode);
    rpt << "\n" << "G::fileSelectionChangeSource = " << G::s(G::fileSelectionChangeSource);
    rpt << "\n" << "G::autoAdvance = " << G::s(G::autoAdvance);
    rpt << "\n";
    rpt << "\n" << "G::maxIconSize = " << G::s(G::maxIconSize);
    rpt << "\n" << "G::minIconSize = " << G::s(G::minIconSize);
    rpt << "\n" << "G::maxIconChunk = " << G::s(G::maxIconChunk);
    rpt << "\n";
    rpt << "\n" << "G::isModifyingDatamodel = " << G::s(G::isModifyingDatamodel);
    rpt << "\n" << "G::ignoreScrollSignal = " << G::s(G::ignoreScrollSignal);
    rpt << "\n" << "G::resizingIcons = " << G::s(G::resizingIcons);
    rpt << "\n" << "G::isSlideShow = " << G::s(G::isSlideShow);
    rpt << "\n" << "G::isRunningColorAnalysis = " << G::s(G::isRunningColorAnalysis);
    rpt << "\n" << "G::isRunningStackOperation = " << G::s(G::isRunningStackOperation);
    rpt << "\n" << "G::isProcessingExportedImages = " << G::s(G::isProcessingExportedImages);
    rpt << "\n" << "G::isEmbellish = " << G::s(G::isEmbellish);
    rpt << "\n" << "G::includeSidecars = " << G::s(G::includeSidecars);
    rpt << "\n" << "G::colorManage = " << G::s(G::colorManage);
    rpt << "\n" << "G::combineRawJpg = " << G::s(G::combineRawJpg);
    rpt << "\n" << "combineRawJpg = " << G::s(combineRawJpg);
    rpt << "\n" << "G::modifySourceFiles = " << G::s(G::modifySourceFiles);
    rpt << "\n" << "G::backupBeforeModifying = " << G::s(G::backupBeforeModifying);
    rpt << "\n" << "G::autoAddMissingThumbnails = " << G::s(G::autoAddMissingThumbnails);
    rpt << "\n" << "G::useSidecar = " << G::s(G::useSidecar);
    rpt << "\n" << "G::renderVideoThumb = " << G::s(G::renderVideoThumb);
    rpt << "\n" << "G::isFilter = " << G::s(G::isFilter);
    rpt << "\n" << "G::isRemote = " << G::s(G::isRemote);
    rpt << "\n";
    rpt << "\n" << "G::isRunningBackgroundIngest = " << G::s(G::isRunningBackgroundIngest);
    rpt << "\n" << "G::ingestCount = " << G::s(G::ingestCount);
    rpt << "\n" << "G::ingestLastSeqDate = " << G::s(G::ingestLastSeqDate);
    rpt << "\n";
    rpt << "\n" << "G::isCopyingFiles = " << G::s(G::isCopyingFiles);
    rpt << "\n" << "G::stopCopyingFiles = " << G::s(G::stopCopyingFiles);
    rpt << "\n";
    rpt << "\n" << "G::isThreadTrackingOn = " << G::s(G::isThreadTrackingOn);
    rpt << "\n" << "G::showAllTableColumns = " << G::s(G::showAllTableColumns);
    rpt << "\n";
    rpt << "\n" << "G::copyCutFileList = " << G::s(G::copyCutFileList);
    rpt << "\n";
    rpt << "\n" << "isShift = " << G::s(isShiftOnOpen);
    rpt << "\n" << "ignoreSelectionChange = " << G::s(ignoreSelectionChange);
    rpt << "\n" << "lastPrefPage = " << G::s(lastPrefPage);
//    rpt << "\n" << "mouseClickScroll = " << G::s(mouseClickScroll);
    rpt << "\n" << "displayPhysicalHorizontalPixels = " << G::s(G::displayPhysicalHorizontalPixels);
    rpt << "\n" << "displayPhysicalVerticalPixels = " << G::s(G::displayPhysicalVerticalPixels);
    rpt << "\n" << "checkIfUpdate = " << G::s(checkIfUpdate);
    rpt << "\n" << "isRatingBadgeVisible = " << G::s(isRatingBadgeVisible);
    rpt << "\n" << "isIconNumberVisible = " << G::s(isIconNumberVisible);
    rpt << "\n" << "classificationBadgeInImageDiameter = " << G::s(classificationBadgeInImageDiameter);
    rpt << "\n" << "classificationBadgeSizeFactor = " << G::s(classificationBadgeSizeFactor);
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
    rpt << "\n" << "cacheSizeMB = " << G::s(imageCache->getMaxMB());
    rpt << "\n" << "showCacheStatus = " << G::s(isShowCacheProgressBar);
    rpt << "\n" << "isShowCacheThreadActivity = " << G::s(isShowCacheProgressBar);
    rpt << "\n" << "progressWidth = " << G::s(cacheBarProgressWidth);
    rpt << "\n" << "fullScreenDocks.isFolders = " << G::s(fullScreenDocks.isFolders);
    rpt << "\n" << "fullScreenDocks.isFavs = " << G::s(fullScreenDocks.isFavs);
    rpt << "\n" << "fullScreenDocks.isFilters = " << G::s(fullScreenDocks.isFilters);
    rpt << "\n" << "fullScreenDocks.isMetadata = " << G::s(fullScreenDocks.isMetadata);
    rpt << "\n" << "fullScreenDocks.isThumbs = " << G::s(fullScreenDocks.isThumbs);
    rpt << "\n" << "fullScreenDocks.isStatusBar = " << G::s(fullScreenDocks.isStatusBar);
    rpt << "\n" << "isNormalScreen = " << G::s(!isFullScreen());
    rpt << "\n" << "dm->primaryFolderPath = " << G::s(dm->primaryFolderPath());
    rpt << "\n" << "prevMode = " << G::s(prevMode);
    rpt << "\n" << "currentRow = " << G::s(dm->currentSfRow);
    rpt << "\n" << "scrollRow = " << G::s(scrollRow);
    // rpt << "\n" << "currentDmIdx = row" << G::s(currDmIdx.row()) << " col " << G::s(currDmIdx.column());
    rpt << "\n" << "modeChangeJustHappened = " << G::s(modeChangeJustHappened);
    rpt << "\n" << "justUpdatedBestFit = " << G::s(justUpdatedBestFit);
    rpt << "\n" << "sortColumn = " << G::s(sortColumn);
    rpt << "\n" << "showImageCount = " << G::s(showImageCount);
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
    rpt << "\n" << "simulateJustInstalled = " << G::s(simulateJustInstalled);
    rpt << "\n" << "isSettings = " << G::s(isSettings);
    rpt << "\n" << "isStressTest = " << G::s(G::isStressTest);
    rpt << "\n" << "hasGridBeenActivated = " << G::s(hasGridBeenActivated);
    rpt << "\n" << "isLeftMouseBtnPressed = " << G::s(isLeftMouseBtnPressed);
    rpt << "\n" << "isMouseDrag = " << G::s(isMouseDrag);
//    rpt << "\n" << "timeToQuit = " << G::s(timeToQuit);
    rpt << "\n" << "sortMenuUpdateToMatchTable = " << G::s(sortMenuUpdateToMatchTable);
    rpt << "\n" << "imageCacheFilePath = " << G::s(imageCacheFilePath);
    rpt << "\n" << "newScrollSignal = " << G::s(newScrollSignal);
    rpt << "\n" << "prevCentralView = " << G::s(prevCentralView);
    rpt << "\n" << "mouseOverFolder = " << G::s(mouseOverFolderPath);
    rpt << "\n" << "rating = " << G::s(rating);
    rpt << "\n" << "colorClass = " << G::s(colorClass);
    rpt << "\n" << "isPick = " << G::s(isPick);

    rpt << "\n\n" ;
    rpt << "\n" << "STYLESHEET css:";
    int n = 140;
    for (int i = 0; i < css.length(); i += n) {
        rpt << "\n" << css.mid(i, n);
    }
    rpt << "\n";
    return reportString;
}

void MW::diagnosticsMain() {diagnosticsReport(this->diagnostics(), "Winnow Diagnostics: MainWindow");}
void MW::diagnosticsSelection() {diagnosticsReport(sel->diagnostics(), "Winnow Diagnostics: Selection");}
void MW::diagnosticsWorkspaces() {diagnosticsReport(this->reportWorkspaces(), "Winnow Diagnostics: WorkSpaces");}
void MW::diagnosticsGridView() {diagnosticsReport(gridView->diagnostics(), "Winnow Diagnostics: GridView");}
void MW::diagnosticsThumbView() {diagnosticsReport(thumbView->diagnostics(), "Winnow Diagnostics: ThumbView");}
void MW::diagnosticsImageView() {diagnosticsReport(imageView->diagnostics(), "Winnow Diagnostics: ImageView");}
void MW::diagnosticsInfoView() {} // dummy for now
void MW::diagnosticsTableView() {} // dummy for now
void MW::diagnosticsCompareView() {} // dummy for now
void MW::diagnosticsMetadata() {
    dm->imMetadata(dm->currentFilePath, true);
    diagnosticsReport(metadata->diagnostics(dm->currentFilePath), "Winnow Diagnostics: Metadata");
}
void MW::diagnosticsXMP() {} // dummy for now
void MW::diagnosticsMetadataCache() {diagnosticsReport(metaRead->diagnostics(), "Winnow Diagnostics: MetaRead");}
void MW::diagnosticsImageCache() {diagnosticsReport(imageCache->diagnostics(), "Winnow Diagnostics: ImageCache");}
void MW::diagnosticsDataModel() {diagnosticsReport(dm->diagnostics(), "Winnow Diagnostics: Data Model");}
void MW::diagnosticsDataModelAllRows() {diagnosticsReport(dm->diagnosticsAllRows(), "Winnow Diagnostics: Data Model All Rows");}
void MW::diagnosticsEmbellish() {diagnosticsReport(embelProperties->diagnostics(), "Winnow Diagnostics: Embellish");}
void MW::diagnosticsFilters() {} // dummy for now
void MW::diagnosticsFSTree() {diagnosticsReport(fsTree->diagnostics(), "Winnow Diagnostics: FSTree");}
void MW::diagnosticsBookmarks() {} // dummy for now
void MW::diagnosticsPixmap() {} // dummy for now
void MW::diagnosticsThumb() {} // dummy for now
void MW::diagnosticsIngest() {} // dummy for now
void MW::diagnosticsZoom() {} // dummy for now

void MW::diagnosticsReport(QString reportString, QString title)
{
    if (G::isLogger) G::log("MW::diagnosticsReport");
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg->winId(), G::backgroundColor);
    #endif
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    md.textBrowser->setStyleSheet(G::css);
    QFont courier("Courier", 12);
    md.textBrowser->setFont(courier);
    md.textBrowser->setText(reportString);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    QFontMetrics metrics(md.textBrowser->font());
    md.textBrowser->setTabStopDistance(3 * metrics.horizontalAdvance(' '));
    fitDiagnostics(dlg, md.textBrowser);
    dlg->setWindowTitle(title);
    openWindows.append(dlg);
    dlg->show();
}

void MW::allIssuesReport()
{
/*
    Show the issue log, which is stored in the file G::issueLogFile
*/
    if (G::isLogger) G::log("MW::allIssuesReport");
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    dlg->setWindowTitle("Winnow Issue Log");
    md.textBrowser->setStyleSheet(G::css);
    md.textBrowser->setFontFamily("Monaco");
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    md.textBrowser->setText(G::issueLog->logText());
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg->winId(), G::backgroundColor);
    #endif
    fitDiagnostics(dlg, md.textBrowser);
    openWindows.append(dlg);
    dlg->show();
}

void MW::SessionIssuesReport()
{
/*
    Show the issues from the current session (from when Winnow was opened)
*/
    if (G::isLogger) G::log("MW::SessionIssuesReport");
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    dlg->setWindowTitle("Winnow Session Issues");
    md.textBrowser->setStyleSheet(G::css);
    md.textBrowser->setFontFamily("Monaco");
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    QString content = G::issueList.join("\n");
    md.textBrowser->setText(content);
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg->winId(), G::backgroundColor);
    #endif

    fitDiagnostics(dlg, md.textBrowser);
    openWindows.append(dlg);
    dlg->show();
}

