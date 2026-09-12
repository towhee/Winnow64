#include "Main/mainwindow.h"
#include "Develop/workingimagecache.h"
#include "Cache/devpreviewcache.h"
#include "Cache/catalog.h"
#include "Cache/cachedb.h"
#include "Metadata/keywordpaths.h"
#include "ui_metadatareport.h"

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX        // keep windows.h min/max macros from breaking std::min/max
#endif
#include <windows.h>
#include <mapi.h>
#include <vector>
#endif

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
        << "\nG::allMetadataAttempted                   " << G::allMetadataAttempted
        << "\nG::iconChunkLoaded                        " << G::iconChunkLoaded
        << "\nG::stop                                " << G::stop
        << "\nG::isFirstImageNewInstance     " << G::isFirstImageNewInstance

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
        << "\nG::currRootFolder                      " << dm->folderList.at(0)
        << "\nccurrentFilePath                       " << dm->currentFilePath
        << "\ncurrentRow                             " << dm->currentSfRow
        << "\ncurrentSfIdx                           " << dm->currentSfIdx
        << "\ncurrentDmIdx                           " << dm->currentDmIdx
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
             << "\nisCatalogDockVisible" << w.isCatalogDockVisible
             << "\nisMetadataDockVisible" << w.isMetadataDockVisible
             << "\nisEmbelDockVisible" << w.isEmbelDockVisible
             << "\nisDevelopDockVisible" << w.isDevelopDockVisible
             << "\nisHistoryDockVisible" << w.isHistoryDockVisible
             << "\nisPresetsDockVisible" << w.isPresetsDockVisible
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
    rpt << "\n" << "G::stop = " << G::s((bool)G::stop);
    rpt << "\n";
    rpt << "\n" << "G::allMetadataAttempted = " << G::s((bool)G::allMetadataAttempted);
    rpt << "\n" << "G::iconChunkLoaded = " << G::s((bool)G::iconChunkLoaded);
    rpt << "\n" << "dm->abortLoadingModel = " << G::s(dm->abort);
    rpt << "\n" << "dm->loadingModel = " << G::s(dm->loadingModel);
    rpt << "\n" << "dm->instance = " << G::s((int)dm->instance);
    rpt << "\n" << "G::dmInstance = " << G::s((int)G::dmInstance);
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
    rpt << "\n" << "G::availableMemoryMB = " << G::s(G::availableMemoryMB.load());
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
    rpt << "\n" << "G::isModifyingDatamodel = " << G::s((bool)G::isModifyingDatamodel);
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
    rpt << "\n" << "updateSkipVersion = " << updateSkipVersion;
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
    rpt << "\n" << "showCacheProgress = " << G::s(G::showCacheProgress);
    rpt << "\n" << "progressWidth = " << G::s(cacheBarProgressWidth);
    rpt << "\n" << "fullScreenDocks.isFolders = " << G::s(fullScreenDocks.isFolders);
    rpt << "\n" << "fullScreenDocks.isFavs = " << G::s(fullScreenDocks.isFavs);
    rpt << "\n" << "fullScreenDocks.isFilters = " << G::s(fullScreenDocks.isFilters);
    rpt << "\n" << "fullScreenDocks.isCatalog = " << G::s(fullScreenDocks.isCatalog);
    rpt << "\n" << "fullScreenDocks.isMetadata = " << G::s(fullScreenDocks.isMetadata);
    rpt << "\n" << "fullScreenDocks.isDevelop = " << G::s(fullScreenDocks.isDevelop);
    rpt << "\n" << "fullScreenDocks.isHistory = " << G::s(fullScreenDocks.isHistory);
    rpt << "\n" << "fullScreenDocks.isPresets = " << G::s(fullScreenDocks.isPresets);
    rpt << "\n" << "fullScreenDocks.isEmbellish = " << G::s(fullScreenDocks.isEmbellish);
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
    for (int i = 0; i < G::css.length(); i += n) {
        rpt << "\n" << G::css.mid(i, n);
    }
    rpt << "\n";
    return reportString;
}

QString MW::developDiagnostics()
{
/*
    A single snapshot of everything relevant to the Develop pipeline for the CURRENTLY
    SELECTED image: the operation-mode / raw-engine globals, the current file, the cached
    scene-linear WorkingImage base, the raw sensor-unpack info, the effective EditParams
    (Base + non-Base scopes), the crop/warp geometry, and the interactive develop caches
    (PMRID "Denoise raw" base + the resolved (k,b) noise-model tier, scene-linear re-decode
    guards, proxy / full-res render state, mask reference caches). Read-only -- builds the
    report from live state and touches no pixels.
*/
    if (G::isLogger) G::log("MW::developDiagnostics");
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);

    rpt << Utilities::centeredRptHdr('=', "Develop Diagnostics");
    rpt << "\n";

    /* One-line EditParams dump helper (only the fields that drive a pixel). */
    auto dumpParams = [&rpt](const EditParams &p, const QString &indent) {
        rpt << "\n" << indent << "isIdentity = " << G::s(p.isIdentity());
        rpt << "\n" << indent << "temp = " << G::s(p.temp) << "   tint = " << G::s(p.tint);
        rpt << "\n" << indent << "exposure = " << G::s(p.exposure)
            << "   contrast = " << G::s(p.contrast);
        rpt << "\n" << indent << "highlights = " << G::s(p.highlights)
            << "   shadows = " << G::s(p.shadows)
            << "   whites = " << G::s(p.whites) << "   blacks = " << G::s(p.blacks);
        rpt << "\n" << indent << "toneShadowCenter = " << G::s(p.toneShadowCenter)
            << "   toneCrossover = " << G::s(p.toneCrossover)
            << "   toneHighlightCenter = " << G::s(p.toneHighlightCenter);
        /* The whole curve set in its shared encoding -- empty when untouched. */
        rpt << "\n" << indent << "curves = "
            << G::s(ToneCurve::encode(p.curveN, p.curveX, p.curveY));
        rpt << "\n" << indent << "texture = " << G::s(p.texture)
            << "   dehaze = " << G::s(p.dehaze);
        rpt << "\n" << indent << "red = " << G::s(p.red) << "   green = " << G::s(p.green)
            << "   blue = " << G::s(p.blue);
        rpt << "\n" << indent << "hue = " << G::s(p.hue) << "   saturation = " << G::s(p.saturation)
            << "   luminance = " << G::s(p.luminance);
        rpt << "\n" << indent << "denoiseLuma (raw) = " << G::s(p.denoiseLuma)
            << "   denoiseChroma (raw) = " << G::s(p.denoiseChroma)
            << "   denoiseRaw = " << (p.denoiseRaw < 0 ? "unset (follows Auto run)"
                                                       : (p.denoiseRaw ? "on" : "off"));
        rpt << "\n" << indent << "localDenoiseLuma = " << G::s(p.localDenoiseLuma)
            << "   localDenoiseChroma = " << G::s(p.localDenoiseChroma);
    };
    auto dims = [](const std::shared_ptr<const WorkingImage> &w) -> QString {
        return w ? QString("%1 x %2").arg(w->width).arg(w->height) : QString("(none)");
    };

    /* Trailing section: every folder holding on-disk devPreviews. Appended by both
       return paths, so it is reported even when no image is selected -- it describes
       the cache, not the current image. */
    auto dumpDevPreviewFolders = [&rpt]() {
        DevPreviewCache &c = DevPreviewCache::instance();
        const QList<DevPreviewCache::FolderStat> folders = c.folderStats();
        auto mb = [](qint64 b) {
            return QString::number(double(b) / (1024.0 * 1024.0), 'f', 1) + " MB";
        };
        rpt << "\n" << Utilities::centeredRptHdr('-', "Folders with cached devPreviews");
        rpt << "\n" << "  cache dir = " << c.cacheDir();
        rpt << "\n" << "  total = " << G::s(c.count()) << " previews in "
            << G::s((int)folders.count()) << " folders   (" << mb(c.totalBytes())
            << " of " << mb(c.maxBytes()) << " cap)";
        /*  WHAT THE HOUSEKEEPING DID THIS SESSION. The three orphan layers run once, off
            the GUI thread, after the first folder load, and until this line there was no
            way to tell whether they had run at all -- a stray payload is not a row, so it
            appears in none of the totals above and shows up only as a cache folder
            larger than the cache says it is. */
        rpt << "\n" << "  housekeeping = " << G::s(c.lastReaped())
            << " demoted rows reaped, " << G::s(c.lastStrays())
            << " stray files removed, " << G::s(c.lastLostRows())
            << " rows with no file dropped";
        if (folders.isEmpty()) {
            rpt << "\n" << "  (no cached devPreviews)";
        }
        for (const DevPreviewCache::FolderStat &f : folders) {
            rpt << "\n" << "  " << f.folder;
            rpt << "\n" << "      previews = " << G::s(f.count)
                << "   live = " << G::s(f.live)
                << "   size = " << mb(f.bytes)
                << (!f.mounted
                        ? "   (volume not mounted -- not swept, so live is unverified)"
                        : f.live < f.count
                              ? "   (missing source images -- deleted or replaced)"
                              : "");
        }
        rpt << "\n";
    };

    // OPERATION MODE / RAW ENGINE
    rpt << "\n" << "OPERATION MODE";
    rpt << "\n" << "  G::operationMode = "
        << (G::operationMode == G::OperationMode::Develop ? "Develop" : "Preview");
    rpt << "\n" << "  G::useRaw = " << G::s(G::useRaw);
    rpt << "\n" << "  G::decodeRawEngine = "
        << (G::decodeRawEngine == G::DecodeRawEngine::winnowDecodeRawEngine ? "Winnow (in-house)"
                                                                            : "Apple (Core Image)");
    rpt << "\n";

    /* PROBES: the switches defined together in global.cpp. Reported as one block so a
       build running with tracing on is obvious here. */
    rpt << "\n" << "PROBES";
    rpt << "\n" << "  G::isPerfProbe = " << G::s(G::isPerfProbe);
    rpt << "\n" << "  G::isIngestProbe = "
        << G::s(G::isIngestProbe.load(std::memory_order_relaxed));
    rpt << "\n" << "  G::isReportDevelopTime = " << G::s(G::isReportDevelopTime);
    rpt << "\n" << "  G::isCopyPathProbe = " << G::s(G::isCopyPathProbe);
    rpt << "\n" << "  G::isWheelProbe = " << G::s(G::isWheelProbe);
    rpt << "\n";

    const QString fPath = dm ? dm->currentFilePath : QString();

    // CURRENT IMAGE
    rpt << "\n" << "CURRENT IMAGE";
    rpt << "\n" << "  currentFilePath = " << (fPath.isEmpty() ? "(none)" : fPath);
    if (fPath.isEmpty()) {
        rpt << "\n" << "  (no image selected -- nothing further to report)";
        rpt << "\n";
        dumpDevPreviewFolders();
        return reportString;
    }
    rpt << "\n" << "  isFileRaw = " << G::s(isFileRaw(fPath));
    rpt << "\n" << "  currentIsVideo = " << G::s(currentIsVideo());
    rpt << "\n" << "  ISO = " << G::s(currentImageIso());
    rpt << "\n" << "  Camera model = "
        << dm->sf->index(dm->currentSfRow, G::CameraModelColumn).data().toString();

    /* WHICH FILE IN THE PREVIEW CACHE IS THIS IMAGE'S? Payloads are named by index row id
       in hex ("537.jpg"), so the mapping is otherwise invisible without querying index.db
       by hand -- and picking the wrong file by eye is easy. The PIXEL SIZE is the point
       of reporting it: a devPreview is meant to be full sensor resolution, and one that
       comes back at the file's embedded-preview size (1024 px wide for an Adobe DNG) says
       the render behind it was not the sensor decode. Read from the JPEG header only.
       This is the mapping, NOT a cache hit: no recipe hash is checked here, so a listed
       file may still be stale for the recipe in force -- DEVELOP PREVIEW below answers
       that. A row naming a file that no longer exists is reported as missing rather than
       hidden, since that is itself worth seeing. */
    const QString devPrevPath = DevPreviewCache::instance().payloadPath(fPath);
    rpt << "\n" << "  devPreview = "
        << (devPrevPath.isEmpty() ? "(none in cache index)" : devPrevPath);
    if (!devPrevPath.isEmpty()) {
        QFileInfo pfi(devPrevPath);
        if (!pfi.exists()) {
            rpt << "\n" << "    (indexed, but the file is MISSING from the cache folder)";
        }
        else {
            const QSize sz = QImageReader(devPrevPath).size();
            rpt << "\n" << "    " << G::s(sz.width()) << " x " << G::s(sz.height())
                << "   " << QString::number(pfi.size() / 1048576.0, 'f', 2) << " MB";
            /* Compare against the sensor, so "is this full size?" needs no arithmetic. */
            RawSensorInfo ri;
            dm->fPathRawInfoGet(fPath, ri);
            if (ri.isRaw && ri.width > 0) {
                const int sensorEdge = qMax(ri.width, ri.height);
                const int prevEdge = qMax(sz.width(), sz.height());
                rpt << "   sensor " << G::s(ri.width) << " x " << G::s(ri.height);
                if (prevEdge * 2 < sensorEdge)
                    rpt << "  <-- FAR SMALLER THAN THE SENSOR (built from the embedded "
                           "preview, not the raw decode?)";
            }
        }
    }
    rpt << "\n";

    // DEVELOP BASE (cached scene-linear WorkingImage)
    auto work = WorkingImageCache::instance().get(fPath);
    rpt << "\n" << "DEVELOP BASE (WorkingImageCache pre-develop image)";
    rpt << "\n" << "  cached = " << (work ? "yes" : "no");
    if (work) {
        const double mb = double(work->rgb.size() * sizeof(float)) / (1024.0 * 1024.0);
        rpt << "\n" << "  dimensions = " << dims(work);
        rpt << "\n" << "  sceneReferred = " << G::s(work->sceneReferred)
            << (work->sceneReferred ? "  (scene-linear sensor decode -- correct for Develop)"
                                    : "  (DISPLAY-REFERRED fallback -- develop is editing the tone-mapped image)");
        rpt << "\n" << "  white = " << G::s(work->white);
        rpt << "\n" << "  rgb floats = " << G::s((int)work->rgb.size())
            << "   (~" << QString::number(mb, 'f', 1) << " MB)";
        rpt << "\n" << "  isValid = " << G::s(work->isValid());
    }
    rpt << "\n";

    // RAW SENSOR INFO
    if (isFileRaw(fPath)) {
        RawSensorInfo ri;
        dm->fPathRawInfoGet(fPath, ri);
        rpt << "\n" << "RAW SENSOR INFO (rawInfo, from metadata read)";
        rpt << "\n" << "  isValid = " << G::s(ri.isValid())
            << "   isRaw = " << G::s(ri.isRaw);
        rpt << "\n" << "  active area = " << G::s(ri.width) << " x " << G::s(ri.height);
        rpt << "\n" << "  bitsPerSample = " << G::s(ri.bitsPerSample)
            << "   compression = " << G::s(ri.compression)
            << "   samplesPerPixel = " << G::s(ri.samplesPerPixel);
        rpt << "\n" << "  stripOffset = " << G::s((int)ri.stripOffset)
            << "   stripLength = " << G::s((int)ri.stripLength);
        rpt << "\n" << "  white = " << G::s((int)ri.white)
            << "   black = [" << G::s((int)ri.black[0]) << ", " << G::s((int)ri.black[1])
            << ", " << G::s((int)ri.black[2]) << ", " << G::s((int)ri.black[3]) << "]";
        rpt << "\n" << "  cfaPlaneColor = [" << G::s((int)ri.cfaPlaneColor[0])
            << ", " << G::s((int)ri.cfaPlaneColor[1]) << ", " << G::s((int)ri.cfaPlaneColor[2])
            << ", " << G::s((int)ri.cfaPlaneColor[3]) << "]  (0=R 1=G 2=B)";
        rpt << "\n" << "  hasColorMatrix = " << G::s(ri.hasColorMatrix);
        rpt << "\n";
    }

    // EDIT PARAMS (Global + masks) + GEOMETRY
    DevelopProperties::StackRenderJob mj = developProperties->stackJob();
    rpt << "\n" << "DEVELOP EDIT PARAMS -- Global scope (applied to the whole image)";
    dumpParams(mj.global, "  ");
    rpt << "\n";
    rpt << "\n" << "MASKS (enabled, in order) = " << G::s((int)mj.scopes.size());
    for (int i = 0; i < mj.scopes.size(); ++i) {
        const DevelopProperties::StackRenderJob::Scope &L = mj.scopes.at(i);
        rpt << "\n" << "  Mask " << G::s(i + 1) << ": components = " << G::s((int)L.components.size())
            << "   combine = " << G::s(L.combine)
            << (L.components.isEmpty() ? "   (no components -> whole image)" : "");
        dumpParams(L.params, "    ");
    }
    rpt << "\n";
    const Geometry &g = mj.geometry;
    rpt << "\n" << "GEOMETRY (crop / straighten / warp -- applied last)";
    rpt << "\n" << "  isIdentity = " << G::s(g.isIdentity()) << "   show = " << G::s(g.show);
    rpt << "\n" << "  crop = x " << G::s(g.cropX) << "  y " << G::s(g.cropY)
        << "  w " << G::s(g.cropW) << "  h " << G::s(g.cropH);
    rpt << "\n" << "  straighten = " << G::s(g.straighten) << " deg"
        << "   hasWarp = " << G::s(g.hasWarp) << "   fillCanvas = " << G::s(g.fillCanvas);
    rpt << "\n";

    // DENOISE / PMRID CACHE STATE
    rpt << "\n" << "DENOISE 'RAW' (PMRID) CACHE STATE";
    rpt << "\n" << "  Global denoiseLuma/Chroma = " << G::s(mj.global.denoiseLuma)
        << " / " << G::s(mj.global.denoiseChroma)
        << "   denoiseRaw = " << (mj.global.denoiseRaw < 0
                                      ? "unset" : (mj.global.denoiseRaw ? "on" : "off"))
        << "   wanted = " << G::s(mj.global.wantsDenoiseRaw(G::autoRunDenoise))
        << (G::decodeRawEngine == G::DecodeRawEngine::winnowDecodeRawEngine
                ? "" : "   (inert: PMRID needs the CFA mosaic, not available on the Apple engine)");
    rpt << "\n" << "  developDenoised = " << dims(developDenoised)
        << "   key = " << (developDenoisedKey.isEmpty() ? "(clean)" : developDenoisedKey);
    rpt << "\n" << "  developPmridFull = " << dims(developPmridFull)
        << "   key = " << (developPmridKey.isEmpty() ? "(none)" : developPmridKey);
    rpt << "\n" << "  developDenoiseInFlightKey = "
        << (developDenoiseInFlightKey.isEmpty() ? "(idle)" : developDenoiseInFlightKey);
    /* Noise model (k,b): the PMRID::resolveKB tier that produced the CURRENT image's base
       (snapshot keyed to developPmridKey, so it is never stale) -- the first thing to
       check if a file denoises wrong. Shown only while developPmridFull is this image. */
    const bool resCurrent = !developPmridResSource.isEmpty() &&
                            developPmridKey.startsWith(fPath + "|");
    if (resCurrent) {
        rpt << "\n" << "  noise model: source = " << developPmridResSource
            << "   k = " << G::s(developPmridResK) << "   b = " << G::s(developPmridResB)
            << "   DNG NoiseProfile = " << (developPmridResHadNP ? "yes" : "no");
    } else {
        rpt << "\n" << "  noise model = (no current PMRID base -- not yet run/rebuilt "
                       "for this image)";
    }
    rpt << "\n";

    // SCENE-LINEAR RE-DECODE + RENDER STATE
    rpt << "\n" << "RENDER / DECODE STATE";
    rpt << "\n" << "  developWorkInFlight = "
        << (developWorkInFlight.isEmpty() ? "(idle)" : developWorkInFlight);
    rpt << "\n" << "  developWorkTriedPath = "
        << (developWorkTriedPath.isEmpty() ? "(none)" : developWorkTriedPath)
        << (developWorkTriedPath == fPath
                ? "   <-- THIS image: scene-linear decode was tried and gave no result (using fallback base)"
                : "");
    rpt << "\n" << "  developParamsGen = " << G::s((int)developParamsGen);
    rpt << "\n" << "  developFullResInFlight = " << G::s(developFullResInFlight);
    rpt << "\n" << "  developProxy = " << dims(developProxy)
        << "   path = " << (developProxyPath.isEmpty() ? "(none)" : developProxyPath);
    rpt << "\n";

    // RENDER VERIFICATION (did the develop pipeline actually change pixels?)
    rpt << "\n" << "RENDER VERIFICATION";

    /* Axis 1: the DISPLAY vs the Preview (embedded) image -- confirms the decode change
       (demosaic) and/or edits altered pixels. Populates on entering Develop, edited or
       not. */
    rpt << "\n" << "  [A] Develop display vs Preview (embedded) -- captured on Develop entry";
    if (developVerifyVsPreviewPath.isEmpty() || developVerifyVsPreviewMaxAbs < 0) {
        rpt << "\n" << "      not run"
            << (developVerifyPreviewBaseline.isNull()
                    ? " (no Preview baseline -- enter Develop from Preview with an image shown)"
                    : " (waiting for the Develop display to render)");
    }
    else {
        const bool stale = (developVerifyVsPreviewPath != fPath);
        rpt << "\n" << "      ran on = " << developVerifyVsPreviewPath
            << (stale ? "   (STALE: not the current image)" : "");
        rpt << "\n" << "      max abs pixel diff (0..255) = " << G::s(developVerifyVsPreviewMaxAbs)
            << (developVerifyVsPreviewMaxAbs > 0
                    ? "   -> PIXELS CHANGED vs Preview (decode/demosaic and/or edits took effect)"
                    : "   -> identical to Preview");
        rpt << "\n" << "      mean abs pixel diff = " << QString::number(developVerifyVsPreviewMeanAbs, 'f', 4);
        if (developVerifyVsPreviewMaxAbs == 0 && isFileRaw(fPath) && G::useRaw)
            rpt << "\n" << "      WARNING: a raw in Edit=Raw is pixel-identical to the embedded preview"
                        << " -- the demosaic/decode did not take effect.";
    }

    /* Axis 2: the recipe vs the un-developed base -- isolates whether the EDITS/denoise
       changed the demosaiced base. Populates on the last full-res settle (edited
       renders). */
    rpt << "\n" << "  [B] recipe vs un-developed base -- last full-res settle";
    if (developVerifyPath.isEmpty() || developVerifyMaxAbs < 0) {
        rpt << "\n" << "      not run yet (settle a full-res render with edits to populate)";
    }
    else {
        const bool stale = (developVerifyPath != fPath);
        rpt << "\n" << "      ran on = " << developVerifyPath << (stale ? "   (STALE: not the current image)" : "");
        rpt << "\n" << "      recipe identity (no non-geometry edits) = " << G::s(developVerifyRecipeIdentity);
        rpt << "\n" << "      geometry active (crop/warp) = " << G::s(developVerifyGeometryActive);
        rpt << "\n" << "      max abs pixel diff (0..255) = " << G::s(developVerifyMaxAbs)
            << (developVerifyMaxAbs > 0 ? "   -> PIXELS CHANGED (recipe is affecting the base)"
                                        : "   -> NO CHANGE (render is pixel-identical to the un-developed base)");
        rpt << "\n" << "      mean abs pixel diff = " << QString::number(developVerifyMeanAbs, 'f', 4);
        if (developVerifyMaxAbs == 0 && !developVerifyRecipeIdentity)
            rpt << "\n" << "      WARNING: a non-identity recipe produced NO pixel change -- an edit is being dropped.";
    }
    rpt << "\n";

    // MASK REFERENCE CACHES
    rpt << "\n" << "MASK REFERENCE CACHES (path currently registered)";
    rpt << "\n" << "  range  = " << (developRangeRefPath.isEmpty()   ? "(none)" : developRangeRefPath);
    rpt << "\n" << "  subject= " << (developSubjectRefPath.isEmpty() ? "(none)" : developSubjectRefPath);
    rpt << "\n" << "  sky    = " << (developSkyRefPath.isEmpty()     ? "(none)" : developSkyRefPath);
    rpt << "\n" << "  depth  = " << (developDepthRefPath.isEmpty()   ? "(none)" : developDepthRefPath);
    rpt << "\n" << "  object = " << (developObjectImagePath.isEmpty()? "(none)" : developObjectImagePath);
    rpt << "\n";

    // FOLDERS WITH CACHED DEVPREVIEWS
    dumpDevPreviewFolders();

    return reportString;
}

void MW::diagnosticsDevelop() {diagnosticsReport(this->developDiagnostics(), "Winnow Diagnostics: Develop");}
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

void MW::diagnosticsMemory()
{
    if (G::isLogger) G::log("MW::diagnosticsMemory");

    // Walk the DataModel splitting icon bytes from the rest. Icons are stored
    // as QIcon at column 0 with Qt::DecorationRole (DataModel::setIcon /
    // setIcon1). All other roles and columns are treated as metadata.
    qint64 iconBytes = 0;
    qint64 metaBytes = 0;
    int    iconRowCount = 0;
    const int rows = dm->rowCount();
    const int cols = dm->columnCount();
    QHash<int, QByteArray> roles = dm->roleNames();
    QList<int> roleKeys = roles.keys();
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            QModelIndex dmIdx = dm->index(row, col);
            for (int role : roleKeys) {
                QVariant data = dm->data(dmIdx, role);
                if (data.isNull()) continue;
                quint64 b = Utilities::qvariantBytes(data);
                if (role == Qt::DecorationRole) {
                    iconBytes += b;
                    if (col == 0 && b > 0) ++iconRowCount;
                } else {
                    metaBytes += b;
                }
            }
        }
    }

    // Refresh OS-side memory stats so the report is current, not from startup.
#ifdef Q_OS_MAC
    Mac::availableMemory();
#elif defined(Q_OS_WIN)
    Win::availableMemory();
#endif

    quint64 totalRamMB = 0;
    int pressureLevel = -1;
#ifdef Q_OS_MAC
    totalRamMB = static_cast<quint64>(Mac::totalMemoryMB());
    pressureLevel = Mac::memoryPressureLevel();
#elif defined(Q_OS_WIN)
    totalRamMB = Win::totalMemoryMB();
#endif

    const quint64 mb           = 1024ull * 1024ull;
    const quint64 iconMB       = static_cast<quint64>(iconBytes) / mb;
    const quint64 metaMB       = static_cast<quint64>(metaBytes) / mb;
    const quint64 imageCacheMB = imageCache ? imageCache->getImCacheSize() : 0;
    const quint64 footprintMB  = G::processFootprintMB();
    const quint64 accountedMB  = iconMB + metaMB + imageCacheMB;
    const qint64  otherMB      = static_cast<qint64>(footprintMB) - static_cast<qint64>(accountedMB);

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "Memory Diagnostics", 80);
    rpt << "\n";

    rpt << "\n" << "Host";
    rpt << "\n" << "  Total physical RAM      = " << totalRamMB << " MB";
    rpt << "\n" << "  Available memory        = " << G::availableMemoryMB << " MB";
    if (pressureLevel >= 0) {
        const char *pStr = pressureLevel == 0 ? "Normal"
                         : pressureLevel == 1 ? "Warning"
                                              : "Critical";
        rpt << "\n" << "  Memory pressure         = " << pStr
            << " (" << pressureLevel << ")";
    }

    rpt << "\n";
    rpt << "\n" << "Process";
    rpt << "\n" << "  phys_footprint          = " << footprintMB << " MB";
    rpt << "\n" << "  memoryAbortMB cap       = " << G::memoryAbortMB << " MB";
    rpt << "\n" << "  memoryOverrunFlag       = "
        << (G::memoryOverrunFlag.load(std::memory_order_relaxed) ? "TRUE" : "false");
    if (G::memoryAbortMB > 0) {
        const double pct = 100.0 * footprintMB / G::memoryAbortMB;
        rpt << "\n" << "  Footprint vs cap        = "
            << QString::number(pct, 'f', 1) << "%";
    }
    if (totalRamMB > 0) {
        const double pct = 100.0 * footprintMB / totalRamMB;
        rpt << "\n" << "  Footprint vs total RAM  = "
            << QString::number(pct, 'f', 1) << "%";
    }

    rpt << "\n";
    rpt << "\n" << "Breakdown (MB)";
    rpt << "\n" << "  DataModel (excl. icons) = " << metaMB;
    rpt << "\n" << "  DataModel icons         = " << iconMB
        << "  (" << iconRowCount << " rows)";
    rpt << "\n" << "  ImageCache              = " << imageCacheMB;
    rpt << "\n" << "  Sum accounted           = " << accountedMB;
    rpt << "\n" << "  Other (footprint - acc) = " << otherMB
        << "   <- Qt widgets, decoders, OS overhead, etc.";

    rpt << "\n";
    rpt << "\n" << "DataModel";
    rpt << "\n" << "  Rows                    = " << rows;
    rpt << "\n" << "  Columns                 = " << cols;
    rpt << "\n" << "  bytesUsed (running)     = " << dm->bytesUsed
        << " (" << (dm->bytesUsed / mb) << " MB)";
    rpt << "\n" << "  metaCacheMB estimate    = " << G::metaCacheMB << " MB";
    rpt << "\n" << "  iconChunkSize           = " << dm->iconChunkSize;
    rpt << "\n" << "  queuedReaderEvents      = "
        << dm->queuedReaderEvents.load(std::memory_order_relaxed);

    rpt << "\n";
    rpt << "\n" << "ImageCache";
    if (imageCache) {
        rpt << "\n" << "  currMB                  = " << imageCacheMB << " MB";
        rpt << "\n" << "  maxMB                   = " << imageCache->getMaxMB() << " MB";
        rpt << "\n" << "  maxMBCeiling            = " << imageCache->getMaxMBCeiling() << " MB";
        rpt << "\n" << "  autoMaxMB               = "
            << (imageCache->getAutoMaxMB() ? "true" : "false");
        rpt << "\n" << "  autoStrategy            = " << imageCache->getAutoStrategy();
        rpt << "\n" << "  isRunning               = "
            << (imageCache->isRunning() ? "true" : "false");
    } else {
        rpt << "\n" << "  (not constructed)";
    }
    rpt << "\n";

    diagnosticsReport(reportString, "Winnow Diagnostics: Memory");
}

void MW::diagnosticsDataModel() {diagnosticsReport(dm->diagnostics(), "Winnow Diagnostics: Data Model");}
void MW::diagnosticsDataModelAllRows() {diagnosticsReport(dm->diagnosticsAllRows(), "Winnow Diagnostics: Data Model All Rows");}
void MW::diagnosticsEmbellish() {diagnosticsReport(embelProperties->diagnostics(), "Winnow Diagnostics: Embellish");}
void MW::diagnosticsFilters() {diagnosticsReport(filters->diagnostics(), "Winnow Diagnostics: Filters");}
void MW::diagnosticsFSTree() {diagnosticsReport(fsTree->diagnostics(), "Winnow Diagnostics: FSTree");}
void MW::diagnosticsBookmarks() {} // dummy for now
void MW::diagnosticsPixmap() {} // dummy for now
void MW::diagnosticsThumb() {} // dummy for now
void MW::toggleIngestProbe()
{
/*
    Arm or disarm the ingest probe (Utilities/ingestprobe.h).

    IT ALSO ARMS THE GUI STALL WATCHDOG, because "the app froze for two seconds" and
    "the loupe was blank" are the same complaint told from either end, and the probe's
    report has a line for stalls that would otherwise always read zero. The watchdog is
    one timer callback four times a second and is never disarmed once started
    (armGuiStallWatchdog is idempotent).

    ARMING CLEARS THE BUFFERS; disarming keeps them, so the report can still be read
    afterwards. That asymmetry is deliberate: a probe is armed to measure what happens
    NEXT, and read after the fact.
*/
    if (G::isLogger) G::log("MW::toggleIngestProbe");
    const bool on = ingestProbeArmAction->isChecked();
    G::isIngestProbe.store(on, std::memory_order_relaxed);
    IngestProbe::Instance().Arm(on);
    /*  The watchdog is armed once and never taken down, so arming the probe LATER has to
        re-tighten its interval -- otherwise it keeps ticking at 250 ms and cannot resolve
        the short pause the probe was armed to find. */
    if (on) armGuiStallWatchdog();
    if (guiStallTimer) guiStallTimer->setInterval(guiStallInterval());
    G::popup->showPopup(on
        ? "Ingest probe recording.<br>Read it at Help > Diagnostics > Ingest probe report."
        : "Ingest probe stopped.  The recording is kept until it is armed again.", 3000);
    updateStatus(true, on ? "Ingest probe recording" : "", "MW::toggleIngestProbe");
}

void MW::diagnosticsIngest()
{
    if (G::isLogger) G::log("MW::diagnosticsIngest");
    diagnosticsReport(IngestProbe::Instance().Report(), "Winnow Diagnostics: Ingest Probe");
}
void MW::diagnosticsZoom() {} // dummy for now

QString MW::keywordDiagnostics()
{
/*
    EVERYTHING THE KEYWORD SYSTEM KNOWS, FROM ITS FOUR STORES AT ONCE, and the point of
    the report is that there ARE four and they are supposed to agree:

      the FILES      dc:subject and lr:hierarchicalSubject, as written
      the DATAMODEL  G::KeywordsColumn / KeywordPathsColumn / KeywordsAllColumn, which is
                     what the Filters panel counts
      the CATALOG    `keyword` + `image_keyword`, the OBSERVED vocabulary, which is what
                     the Keywords dock and every cross-folder search count
      the VOCAB      the `vocab` tree the user curates, which is what decides whether a
                     keyword reads as filed or unfiled

    EVERY KEYWORD BUG SO FAR HAS BEEN A DISAGREEMENT BETWEEN TWO OF THEM, and each was
    found late and by accident -- a keyword reading 574 images in one panel and 1 in the
    other (schema 12), links that had drifted from the text they are derived from (schema
    11 and 12 both), a rename that never persisted because synonyms bound as null. None of
    them announced itself. So the report leads with the comparisons rather than with the
    inventory: the drift audit, the two counts of the same keyword side by side, and what
    the current image carries at each of the four levels.

    READ-ONLY AND ON DEMAND. It queries the catalog and walks the vocabulary on the GUI
    thread, which is why it is a menu item and not something a load path calls. The drift
    audit is sampled (see Catalog::keywordAudit) so the cost stays bounded on a library.
*/
    if (G::isLogger) G::log("MW::keywordDiagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);

    rpt << Utilities::centeredRptHdr('=', "Keyword Diagnostics");
    rpt << "\n";

    Catalog &cat = Catalog::instance();
    const bool haveCat = cat.isAvailable();

    /* ---------------------------------------------------------------------------
       Context
       --------------------------------------------------------------------------- */
    rpt << "\n" << "Context";
    rpt << "\n" << "  Database                = " << CacheDb::instance().path();
    rpt << "\n" << "  Catalog available       = " << G::s(haveCat);
    rpt << "\n" << "  Scope                   = "
        << (G::scope == G::Scope::Catalog ? "Catalog" : "Folders");
    rpt << "\n" << "  Current folder          = "
        << (dm ? dm->primaryFolderPath() : QString());
    rpt << "\n" << "  Datamodel rows          = " << G::s(dm ? dm->rowCount() : 0);
    rpt << "\n" << "  Keywords dock           = "
        << (keywordsDock ? (keywordsDock->isVisible() ? "visible" : "hidden")
                         : "not constructed");
    rpt << "\n" << "  Dock refresh pending    = " << G::s(keywordsDockRefreshPending);
    rpt << "\n";

    /* ---------------------------------------------------------------------------
       The catalog tables, and the drift audit that is the reason for this report
       --------------------------------------------------------------------------- */
    const KeywordAudit a = cat.keywordAudit();

    rpt << "\n" << "Catalog keyword tables (the OBSERVED vocabulary)";
    if (!a.available) {
        rpt << "\n" << "  (database not open -- nothing to report)";
    }
    else {
        rpt << "\n" << "  Schema version          = " << G::s(a.schemaVersion)
            << "   (code expects " << G::s(CacheDb::schemaVersion()) << ")";
        rpt << "\n" << "  Live images             = " << G::s(a.liveImages);
        rpt << "\n" << "  ... carrying keyword text = " << G::s(a.imagesWithText);
        rpt << "\n" << "  ... carrying links        = " << G::s(a.imagesWithLinks);
        rpt << "\n" << "  keyword rows            = " << G::s(a.keywordRows);
        rpt << "\n" << "  vocab rows              = " << G::s(a.vocabRows);
        rpt << "\n" << "  image_keyword links     = " << G::s(a.links);
        rpt << "\n" << "  Keywords nothing links to = " << G::s(a.unlinkedKeywords)
            << "   (prunable)";
        rpt << "\n" << "  Links to a missing keyword = " << G::s(a.orphanLinksNoKeyword);
        rpt << "\n" << "  Links to a missing image   = " << G::s(a.orphanLinksNoImage);
        rpt << "\n" << "  Images with links and no text = " << G::s(a.linksWithoutText)
            << "   (not rebuildable from the file)";
    }
    rpt << "\n";

    rpt << "\n" << "Link drift: do an image's links match the text they are derived from?";
    if (!a.available) {
        rpt << "\n" << "  (not checked)";
    }
    else if (a.sampled == 0) {
        rpt << "\n" << "  No image carries keyword text -- nothing to compare.";
    }
    else {
        rpt << "\n" << "  Images sampled          = " << G::s(a.sampled)
            << " of " << G::s(a.imagesWithText);
        rpt << "\n" << "  Images that disagree    = " << G::s(a.drifted);
        rpt << "\n" << "  Links missing           = " << G::s(a.missingLinks);
        rpt << "\n" << "  Links the text does not carry = " << G::s(a.extraLinks);
        if (a.drifted == 0) {
            rpt << "\n" << "  CLEAN -- every sampled image's links are the prefix";
            rpt << "\n" << "  expansion of its own keywords_literal + keywordpaths.";
        }
        else {
            rpt << "\n";
            rpt << "\n" << "  THIS IS THE FAULT SCHEMA 11 AND 12 REPAIRED. A rescan cannot fix it:";
            rpt << "\n" << "  the text and the freshness stamps are correct, so commit() skips";
            rpt << "\n" << "  the row. Examples:";
            for (const QString &e : a.examples) rpt << "\n" << "    " << e;
        }
    }
    rpt << "\n";

    /* ---------------------------------------------------------------------------
       The authored vocabulary, walked
       --------------------------------------------------------------------------- */
    rpt << "\n" << "Authored vocabulary (the `vocab` tree, what the user curates)";
    if (!keywordVocab) {
        rpt << "\n" << "  (not constructed)";
    }
    else if (keywordVocab->rowCount(QModelIndex()) == 0) {
        rpt << "\n" << "  NOT LOADED (or empty). It loads lazily -- opening the Keywords";
        rpt << "\n" << "  panel or a filter build fills it.";
    }
    else {
        int nodes = 0, roots = 0, leaves = 0, deepest = 0;
        int withSynonyms = 0, notExportable = 0, zeroCount = 0;
        /*  Folded leaf -> the paths using it, so a name living under two parents can be
            named rather than only counted. That ambiguity is the whole reason keyword
            identity is the path, and 59 of one library's 3,975 names had it. */
        QMap<QString, QStringList> leafUse;
        QSet<QString> vocabFold;

        std::function<void(const QModelIndex &, int)> walk =
            [&](const QModelIndex &parent, int depth) {
            const int n = keywordVocab->rowCount(parent);
            for (int i = 0; i < n; ++i) {
                const QModelIndex idx = keywordVocab->index(i, 0, parent);
                const QString path = idx.data(KeywordVocab::PathRole).toString();
                nodes++;
                if (depth == 1) roots++;
                if (depth > deepest) deepest = depth;
                if (keywordVocab->rowCount(idx) == 0) leaves++;
                if (!idx.data(KeywordVocab::SynonymsRole).toStringList().isEmpty())
                    withSynonyms++;
                if (!idx.data(KeywordVocab::ExportableRole).toBool()) notExportable++;
                if (idx.data(KeywordVocab::CountRole).toInt() == 0) zeroCount++;
                if (!path.isEmpty()) {
                    vocabFold.insert(keywordFold(path));
                    leafUse[keywordFold(keywordLeafOf(path))] << path;
                }
                walk(idx, depth + 1);
            }
        };
        walk(QModelIndex(), 1);

        rpt << "\n" << "  Nodes                   = " << G::s(nodes);
        rpt << "\n" << "  Roots                   = " << G::s(roots);
        rpt << "\n" << "  Leaves                  = " << G::s(leaves);
        rpt << "\n" << "  Deepest branch          = " << G::s(deepest);
        rpt << "\n" << "  With synonyms           = " << G::s(withSynonyms);
        rpt << "\n" << "  Not exportable          = " << G::s(notExportable);
        rpt << "\n" << "  No images (empty branch) = " << G::s(zeroCount)
            << "   (legitimate here, impossible in `keyword`)";

        int ambiguous = 0;
        QStringList ambiguousLines;
        for (auto it = leafUse.constBegin(); it != leafUse.constEnd(); ++it) {
            if (it.value().size() < 2) continue;
            ambiguous++;
            if (ambiguousLines.size() < 10)
                ambiguousLines << QString("    %1").arg(it.value().join("   |   "));
        }
        rpt << "\n" << "  Names under >1 parent   = " << G::s(ambiguous)
            << "   (why identity is the PATH)";
        for (const QString &l : ambiguousLines) rpt << "\n" << l;

        /*  THE TWO VOCABULARIES COMPARED. An observed path with no authored node is an
            UNFILED keyword -- the thing the Filters panel draws in italic and the
            Keywords dock offers to merge. An authored node the catalog has never seen is
            the opposite and is not a fault: an empty branch made room for. */
        if (haveCat) {
            const QList<CatalogKeyword> observed = cat.keywords();
            QList<CatalogKeyword> unfiled;
            QSet<QString> observedFold;
            for (const CatalogKeyword &k : observed) {
                observedFold.insert(keywordFold(k.path));
                if (!vocabFold.contains(keywordFold(k.path))) unfiled << k;
            }
            int authoredOnly = 0;
            for (const QString &f : vocabFold)
                if (!observedFold.contains(f)) authoredOnly++;

            std::sort(unfiled.begin(), unfiled.end(),
                      [](const CatalogKeyword &x, const CatalogKeyword &y) {
                          return x.count > y.count;
                      });

            rpt << "\n";
            rpt << "\n" << "  Observed paths            = " << G::s(observed.size());
            rpt << "\n" << "  ... unfiled (no vocab node) = " << G::s(unfiled.size());
            rpt << "\n" << "  Authored nodes never observed = " << G::s(authoredOnly)
                << "   (empty branches -- not a fault)";
            rpt << "\n" << "  Filters panel unfiled count = "
                << G::s(filters ? filters->unfiledKeywordCount() : -1);
            if (!unfiled.isEmpty()) {
                rpt << "\n" << "  Biggest unfiled keywords:";
                for (int i = 0; i < unfiled.size() && i < 15; ++i)
                    rpt << "\n" << QString("    %1  %2")
                               .arg(unfiled.at(i).count, 7).arg(unfiled.at(i).path);
            }
        }
    }
    rpt << "\n";

    /* ---------------------------------------------------------------------------
       Flat keywords -- what the tidy operation would see
       --------------------------------------------------------------------------- */
    rpt << "\n" << "Flat keywords (carried as a ROOT after leaf consumption)";
    if (!haveCat) {
        rpt << "\n" << "  (no catalog)";
    }
    else {
        const QList<CatalogKeyword> flat = cat.flatKeywords();
        rpt << "\n" << "  Distinct flat keywords  = " << G::s(flat.size());
        rpt << "\n" << "  (Tidy flat keywords... files these into the tree)";
        for (int i = 0; i < flat.size() && i < 15; ++i)
            rpt << "\n" << QString("    %1  %2")
                       .arg(flat.at(i).count, 7).arg(flat.at(i).path);
        if (flat.size() > 15)
            rpt << "\n" << QString("    ... and %1 more").arg(flat.size() - 15);
    }
    rpt << "\n";

    /* ---------------------------------------------------------------------------
       The two counts of one keyword, side by side -- the disagreement that started it
       --------------------------------------------------------------------------- */
    rpt << "\n" << "Counts compared: Filters (datamodel) vs catalog (links)";
    if (!haveCat || !filters) {
        rpt << "\n" << "  (needs the catalog and the Filters panel)";
    }
    else if (G::scope != G::Scope::Catalog) {
        rpt << "\n" << "  Folders scope: the two SHOULD differ -- the catalog counts the";
        rpt << "\n" << "  whole library and the datamodel holds one folder. Not compared.";
    }
    else {
        const QList<CatalogKeyword> observed = cat.keywords();
        int compared = 0, disagree = 0;
        QStringList lines;
        for (const CatalogKeyword &k : observed) {
            const int inFilters = filters->keywordItemCount(k.path, /*filtered*/ false);
            if (inFilters < 0) continue;        // no item: nothing was claimed
            compared++;
            if (inFilters == k.count) continue;
            disagree++;
            if (lines.size() < 20)
                lines << QString("    %1  filters %2   catalog %3")
                             .arg(k.path, -60).arg(inFilters, 7).arg(k.count, 7);
        }
        rpt << "\n" << "  Keywords compared       = " << G::s(compared);
        rpt << "\n" << "  Keywords that disagree  = " << G::s(disagree);
        for (const QString &l : lines) rpt << "\n" << l;
        if (disagree > lines.size())
            rpt << "\n" << QString("    ... and %1 more").arg(disagree - lines.size());
    }
    rpt << "\n";

    /* ---------------------------------------------------------------------------
       The current image at all four levels
       --------------------------------------------------------------------------- */
    rpt << "\n" << "Current image";
    const int dmRow = (dm && dm->currentSfRow >= 0)
                          ? dm->modelRowFromProxyRow(dm->currentSfRow) : -1;
    if (dmRow < 0) {
        rpt << "\n" << "  (no current image)";
    }
    else {
        const QString fPath = dm->currentFilePath;
        const QStringList literal =
            dm->index(dmRow, G::KeywordsColumn).data().toStringList();
        const QStringList paths =
            dm->index(dmRow, G::KeywordPathsColumn).data().toStringList();
        const QStringList all =
            dm->index(dmRow, G::KeywordsAllColumn).data().toStringList();
        const QStringList effective = keywordEffectivePaths(literal, paths);
        const QStringList expanded = keywordPrefixExpand(effective);

        rpt << "\n" << "  Path                    = " << fPath;
        rpt << "\n" << "  dc:subject (literal)    = " << literal.join(", ");
        rpt << "\n" << "  lr:hierarchicalSubject  = " << paths.join(", ");
        rpt << "\n" << "  Effective paths         = " << effective.join(", ");
        rpt << "\n" << "  Prefix expansion        = " << expanded.join(", ");
        rpt << "\n" << "  KeywordsAllColumn       = " << all.join(", ");
        /*  The expansion is what the commit derives the links from, so a column
            that differs from it is the datamodel-side half of the same drift the
            audit above measures in the database. */
        rpt << "\n" << "  Column matches expansion = "
            << G::s(QSet<QString>(all.constBegin(), all.constEnd())
                    == QSet<QString>(expanded.constBegin(), expanded.constEnd()));
        if (keywordVocab) {
            QStringList unfiledHere;
            for (const QString &p : effective)
                if (!keywordVocab->indexForPath(p).isValid()) unfiledHere << p;
            rpt << "\n" << "  Not in the vocabulary   = "
                << (unfiledHere.isEmpty() ? "(none)" : unfiledHere.join(", "));
        }
    }
    rpt << "\n";

    /* ---------------------------------------------------------------------------
       The selection -- what the dock's tag zone is showing
       --------------------------------------------------------------------------- */
    const QMap<QString, int> inSel = keywordsInSelection();
    const int selCount = dm ? dm->selectionModel->selectedRows().size() : 0;
    rpt << "\n" << "Selection (" << G::s(selCount) << " images, "
        << G::s(inSel.size()) << " distinct keywords)";
    if (inSel.isEmpty()) {
        rpt << "\n" << "  (no keywords on the selection)";
    }
    else {
        for (auto it = inSel.constBegin(); it != inSel.constEnd(); ++it)
            rpt << "\n" << QString("    %1  %2%3")
                       .arg(it.value(), 7).arg(it.key())
                       .arg(it.value() < selCount ? "   (on some)" : "");
    }
    rpt << "\n";

    /* ---------------------------------------------------------------------------
       The keyword list itself, indented -- last, because it is the long one
       --------------------------------------------------------------------------- */
    rpt << "\n" << "Keyword list (indented, as the Keywords dock draws it)";
    rpt << "\n" << "  Columns: node, images carrying it (subtree total -- every image is";
    rpt << "\n" << "  linked to every ancestor), then markers:";
    rpt << "\n" << "    ~  has synonyms      x  not exportable      ?  not in the catalog";
    if (keywordVocab && keywordVocab->rowCount(QModelIndex()) > 0) {
        /*  THE MODEL IS WALKED, NOT THE CATALOG, so the order and the shape are the
            dock's own -- a report that re-derived the tree from paths could differ from
            the panel the user is looking at, which is the one thing it must not do. */
        std::function<void(const QModelIndex &, int)> draw =
            [&](const QModelIndex &parent, int depth) {
            const int n = keywordVocab->rowCount(parent);
            for (int i = 0; i < n; ++i) {
                const QModelIndex idx = keywordVocab->index(i, 0, parent);
                const QString name = idx.data(Qt::DisplayRole).toString();
                const int count = idx.data(KeywordVocab::CountRole).toInt();
                QString marks;
                if (!idx.data(KeywordVocab::SynonymsRole).toStringList().isEmpty())
                    marks += " ~";
                if (!idx.data(KeywordVocab::ExportableRole).toBool()) marks += " x";
                if (count == 0) marks += " ?";
                const QString indented = QString(4 + depth * 2, ' ') + name;
                rpt << "\n" << indented.leftJustified(70, ' ')
                    << QString("%1").arg(count, 7) << marks;
                draw(idx, depth + 1);
            }
        };
        draw(QModelIndex(), 0);
    }
    else if (haveCat) {
        /*  NO AUTHORED VOCABULARY TO DRAW, so the OBSERVED one stands in -- which is the
            state a user who has never opened the Keywords dock is actually in, and the
            one where "show me the keyword list" is least likely to be answered by
            anything else on screen. Catalog::keywords() comes back ordered by pathfold,
            and that IS depth-first order for a tree: a parent's path is its children's
            prefix, so it sorts immediately before them and the indent needs no sort of
            its own. */
        rpt << "\n" << "  (the authored vocabulary is not loaded -- showing the OBSERVED";
        rpt << "\n" << "  keywords from the catalog instead)";
        for (const CatalogKeyword &k : cat.keywords()) {
            const int depth = keywordNodes(k.path).size() - 1;
            const QString indented = QString(4 + depth * 2, ' ') + k.name;
            rpt << "\n" << indented.leftJustified(70, ' ')
                << QString("%1").arg(k.count, 7);
        }
    }
    else {
        rpt << "\n" << "  (no vocabulary loaded and no catalog -- nothing to list)";
    }
    rpt << "\n";

    return reportString;
}

void MW::diagnosticsKeywords()
{
/*
    THE BUSY CURSOR IS NOT DECORATION. The report runs a drift audit over as many as
    20,000 images and two whole-vocabulary queries, which on a library is seconds of a
    window that is otherwise doing nothing visible.
*/
    if (G::isLogger) G::log("MW::diagnosticsKeywords");
    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    const QString rpt = this->keywordDiagnostics();
    QGuiApplication::restoreOverrideCursor();
    diagnosticsReport(rpt, "Winnow Diagnostics: Keywords");
}

void MW::diagnosticsReport(QString reportString, QString title)
{
    ReportDialog *dlg = new ReportDialog(this);
    dlg->setReport(title, reportString);

    // fitDiagnostics still works because we provided a browser() getter
    fitDiagnostics(dlg, dlg->browser());

    openWindows.append(dlg);
    dlg->show();
    dlg->setFocus();
}

void MW::allIssuesReport()
{
/*
    Show the issue log, which is stored in the file G::issueLogFile
*/
    if (G::issueLog == nullptr) return;
    diagnosticsReport(G::issueLog->logText(), "Winnow Issues");
}

void MW::sessionIssuesReport()
{
/*
    Show the issues from the current session (from when Winnow was opened).
    Appends a "repeated" summary from issueDedup so collapsed floods still surface.
*/
    if (G::isLogger) G::log("MW::SessionIssuesReport");
    QString body = G::issueList.join("\n");
    QStringList dedup = G::issueDedupReport();
    if (!dedup.isEmpty()) {
        body += "\n\n----- Repeated (collapsed) issues -----\n";
        body += dedup.join("\n");
    }
    diagnosticsReport(body, "Winnow Session Issues");
}

void MW::logReport()
{
/*
    Show the issues from the current session (from when Winnow was opened)
*/
    if (G::isLogger) G::log("MW::logReport");

    QString content;
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log/WinnowLog.txt";
    QFile logFile(path);

    qDebug() << "MW::logReport  file.exists =" << logFile.exists() << logFile.isOpen();
    if (logFile.isOpen()) logFile.close();

    if (logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&logFile);
        content = in.readAll();
        logFile.close();
        if (content.length())
            diagnosticsReport(content, "Winnow Log");
        else
            G::popup->showPopup("The log is empty.", 2000);
    } else {
        G::popup->showPopup("Failed to open log.", 2000);
    }

}

void MW::mailLogs()
{
/*
    Compose an email to the Winnow developer with the two log files attached:
    WinnowIssueLog.txt and WinnowLog.txt, both located in
    QStandardPaths::AppDataLocation + "/Log".

    A mailto: URL cannot attach files (clients ignore the attachment
    parameter), so each platform uses a native mechanism for real attachments:
        - macOS:   drive Mail.app via AppleScript.
        - Windows: Simple MAPI (MAPISendMail) attaches to the default client.
    On any other platform (or if MAPI is unavailable) we fall back to revealing
    the log folder and opening a pre-filled mailto: that asks the user to attach
    the files manually.
*/
    if (G::isLogger) G::log("MW::mailLogs");

    const QString to = "winnowimageviewer@outlook.com";
    const QString subject = "Winnow log files";
    const QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";

    // Only attach logs that actually exist.
    QStringList logPaths;
    for (const QString &name : {QStringLiteral("WinnowIssueLog.txt"), QStringLiteral("WinnowLog.txt")}) {
        QString path = logDir + "/" + name;
        if (QFile::exists(path)) logPaths << path;
    }
    if (logPaths.isEmpty()) {
        G::popup->showPopup("No log files were found in " + logDir, 3000);
        return;
    }

#if defined(Q_OS_MAC)
    QString body = "The Winnow log files are attached. Please add a note describing the issue. Thanks. Rory";
    QStringList args;
    args << "-e" << "tell application \"Mail\"";
    args << "-e" << "set newMessage to make new outgoing message with properties "
                    "{subject:\"" + subject + "\", content:\"" + body + "\", visible:true}";
    args << "-e" << "tell newMessage";
    args << "-e" << "make new to recipient at end of to recipients with properties "
                    "{address:\"" + to + "\"}";
    for (const QString &path : logPaths) {
        args << "-e" << "make new attachment with properties {file name:(POSIX file \"" + path +
                        "\")} at after the last paragraph of content";
    }
    args << "-e" << "end tell";
    args << "-e" << "activate";
    args << "-e" << "end tell";

    if (QProcess::execute("/usr/bin/osascript", args) != 0)
        G::popup->showPopup("Unable to open Mail to send the log files.", 3000);
#else
    // Reveal the log folder and open a pre-filled email asking the user to
    // attach the files manually. Used as the fallback on platforms without a
    // native attach path, or when MAPI is unavailable on Windows.
    auto revealAndMailto = [&] {
        revealInFileBrowser(logDir);
        QString body = "Please attach the log files shown in the file browser ("
                       + QStringList(logPaths).replaceInStrings(logDir + "/", "").join(" and ")
                       + ") to this email, along with a note describing the issue. Thanks. Rory";
        QDesktopServices::openUrl(QUrl("mailto:" + to + "?subject=" + subject + "&body=" + body,
                                       QUrl::TolerantMode));
    };

  #if defined(Q_OS_WIN)
    // Simple MAPI attaches the logs to a new message in the default mail
    // client. mapi32.dll is loaded dynamically so the build needs no extra
    // link dependency; if it is missing we fall back to the manual path.
    typedef ULONG (PASCAL *LPMAPISENDMAIL)(LHANDLE, ULONG_PTR, lpMapiMessage, FLAGS, ULONG);
    HMODULE hMapi = LoadLibraryA("MAPI32.DLL");
    LPMAPISENDMAIL mapiSendMail = hMapi
        ? reinterpret_cast<LPMAPISENDMAIL>(GetProcAddress(hMapi, "MAPISendMail"))
        : nullptr;

    if (!mapiSendMail) {
        if (hMapi) FreeLibrary(hMapi);
        revealAndMailto();
        return;
    }

    // Simple MAPI is ANSI; keep every char buffer alive until the call returns.
    QByteArray toAddrBuf = ("SMTP:" + to).toLocal8Bit();
    QByteArray toNameBuf = to.toLocal8Bit();
    QByteArray subjectBuf = subject.toLocal8Bit();
    QByteArray bodyBuf = "The Winnow log files are attached. Please add a note "
                         "describing the issue. Thanks. Rory";

    MapiRecipDesc recip = {};
    recip.ulRecipClass = MAPI_TO;
    recip.lpszName = toNameBuf.data();
    recip.lpszAddress = toAddrBuf.data();

    std::vector<QByteArray> pathBufs, nameBufs;
    pathBufs.reserve(logPaths.size());          // reserve so data() stays valid
    nameBufs.reserve(logPaths.size());
    std::vector<MapiFileDesc> files(logPaths.size());
    for (int i = 0; i < logPaths.size(); ++i) {
        pathBufs.push_back(QDir::toNativeSeparators(logPaths.at(i)).toLocal8Bit());
        nameBufs.push_back(QFileInfo(logPaths.at(i)).fileName().toLocal8Bit());
        files[i].nPosition = static_cast<ULONG>(-1);   // attach, not inline
        files[i].lpszPathName = pathBufs.back().data();
        files[i].lpszFileName = nameBufs.back().data();
    }

    MapiMessage msg = {};
    msg.lpszSubject = subjectBuf.data();
    msg.lpszNoteText = bodyBuf.data();
    msg.nRecipCount = 1;
    msg.lpRecips = &recip;
    msg.nFileCount = static_cast<ULONG>(files.size());
    msg.lpFiles = files.data();

    ULONG rc = mapiSendMail(0, static_cast<ULONG_PTR>(winId()), &msg,
                            MAPI_LOGON_UI | MAPI_DIALOG, 0);
    FreeLibrary(hMapi);

    if (rc != SUCCESS_SUCCESS && rc != MAPI_E_USER_ABORT)
        G::popup->showPopup("Unable to open the mail client to send the log files.", 3000);
  #else
    revealAndMailto();
  #endif
#endif
}
