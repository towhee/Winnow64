#include "Main/mainwindow.h"

void MW::writeSettings()
{
/*
    This function is called when exiting the application.

    Using QSetting, write the persistent application settings so they can be re-established
    when the application is re-opened.
*/
    if (G::isLogger) G::log("MW::writeSettings");

    // general
    setting->setValue("lastPrefPage", lastPrefPage);
//    setting->setValue("mouseClickScroll", mouseClickScroll);
    setting->setValue("toggleZoomValue", imageView->toggleZoom);
    setting->setValue("limitFit100Pct", imageView->limitFit100Pct);
//    setting->setValue("sortColumn", sortColumn);
//    setting->setValue("sortReverse", sortReverseAction->isChecked());
    setting->setValue("autoAdvance", autoAdvance);
    setting->setValue("turnOffEmbellish", turnOffEmbellish);
    setting->setValue("deleteWarning", deleteWarning);
    setting->setValue("modifySourceFiles", G::modifySourceFiles);
    setting->setValue("backupBeforeModifying", G::backupBeforeModifying);
    setting->setValue("autoAddMissingThumbnails", G::autoAddMissingThumbnails);
    setting->setValue("ignoreAddThumbnailsDlg", ignoreAddThumbnailsDlg);
    setting->setValue("useSidecar", G::useSidecar);
//    setting->setValue("embedTifThumb", G::embedTifJpgThumb);
    setting->setValue("renderVideoThumb", G::renderVideoThumb);
    setting->setValue("isLogger", G::isLogger);
    setting->setValue("isErrorLogger", G::isErrorLogger);
    setting->setValue("wheelSensitivity", G::wheelSensitivity);

    // datamodel
    setting->setValue("maxIconSize", G::maxIconSize);

    // appearance
    setting->setValue("backgroundShade", G::backgroundShade);
    setting->setValue("fontSize", G::strFontSize);
    setting->setValue("classificationBadgeInImageDiameter", classificationBadgeInImageDiameter);
    setting->setValue("classificationBadgeSizeFactor", classificationBadgeSizeFactor);
    setting->setValue("iconNumberSize", iconNumberSize);
    setting->setValue("infoOverlayFontSize", imageView->infoOverlayFontSize);

    // files
    setting->setValue("colorManage", G::colorManage);
    setting->setValue("rememberLastDir", rememberLastDir);
    setting->setValue("checkIfUpdate", checkIfUpdate);
//    setting->setValue("includeSubfolders", subFoldersAction->isChecked());
    setting->setValue("combineRawJpg", combineRawJpg);

    /* ingest (moved to MW::ingest)
    */

    // thumbs
    setting->setValue("thumbWidth", thumbView->iconWidth);
    setting->setValue("thumbHeight", thumbView->iconHeight);
    setting->setValue("labelFontSize", thumbView->labelFontSize);
    setting->setValue("showThumbLabels", thumbView->showIconLabels);
    setting->setValue("showZoomFrame", thumbView->showZoomFrame);

    // grid
    setting->setValue("thumbWidthGrid", gridView->iconWidth);
    setting->setValue("thumbHeightGrid", gridView->iconHeight);
    setting->setValue("labelFontSizeGrid", gridView->labelFontSize);
    setting->setValue("showThumbLabelsGrid", gridView->showIconLabels);
    setting->setValue("labelChoice", gridView->labelChoice);

    setting->setValue("thumbsPerPage", metadataCacheThread->visibleIcons);

    // slideshow
    setting->setValue("slideShowDelay", slideShowDelay);
    setting->setValue("isSlideShowRandom", isSlideShowRandom);
    setting->setValue("isSlideShowWrap", isSlideShowWrap);

    // metadata and icon cache
    setting->setValue("cacheAllMetadata", metadataCacheThread->cacheAllMetadata);
    setting->setValue("cacheAllIcons", metadataCacheThread->cacheAllIcons);
//    setting->setValue("iconChunkSize", dm->iconChunkSize);

    // image cache
    setting->setValue("cacheMethod", cacheMethod);
    setting->setValue("cacheSizeMethod", cacheSizeStrategy);
    setting->setValue("cacheMinSize", cacheMinSize);
    setting->setValue("isShowCacheStatus", isShowCacheProgressBar);
    setting->setValue("cacheDelay", cacheDelay);
    setting->setValue("cacheStatusWidth", progressWidth);
    setting->setValue("cacheWtAhead", cacheWtAhead);
    setting->setValue("isCachePreview", isCachePreview);
    setting->setValue("cachePreviewWidth", cachePreviewWidth);
    setting->setValue("cachePreviewHeight", cachePreviewHeight);

    // full screen
    setting->setValue("isFullScreenFolders", fullScreenDocks.isFolders);
    setting->setValue("isFullScreenFavs", fullScreenDocks.isFavs);
    setting->setValue("isFullScreenFilters", fullScreenDocks.isFilters);
    setting->setValue("isFullScreenMetadata", fullScreenDocks.isMetadata);
    setting->setValue("isFullScreenThumbs", fullScreenDocks.isThumbs);
    setting->setValue("isFullScreenStatusBar", fullScreenDocks.isStatusBar);

    // state
    setting->setValue("Geometry", saveGeometry());
    setting->setValue("WindowState", saveState());
    setting->setValue("WindowLocation", geometry());
    setting->setValue("isFullScreen", isFullScreen());

    setting->setValue("isRatingBadgeVisible", ratingBadgeVisibleAction->isChecked());
    setting->setValue("isIconNumberVisible", iconNumberVisibleAction->isChecked());
    setting->setValue("isImageInfoVisible", infoVisibleAction->isChecked());
    setting->setValue("isLoupeDisplay", asLoupeAction->isChecked());
    setting->setValue("isGridDisplay", asGridAction->isChecked());
    setting->setValue("isTableDisplay", asTableAction->isChecked());
    setting->setValue("isCompareDisplay", asCompareAction->isChecked());

    setting->setValue("wasThumbDockVisible", wasThumbDockVisible);

    setting->setValue("isMenuBarVisible", menuBarVisibleAction->isChecked());
    setting->setValue("isStatusBarVisible", statusBarVisibleAction->isChecked());
    setting->setValue("isFolderDockVisible", folderDockVisibleAction->isChecked());
    setting->setValue("isFavDockVisible", favDockVisibleAction->isChecked());
    setting->setValue("isFilterDockVisible", filterDockVisibleAction->isChecked());
    setting->setValue("isMetadataDockVisible", metadataDockVisibleAction->isChecked());
    setting->setValue("isEmbelDockVisible", embelDockVisibleAction->isChecked());
    setting->setValue("isThumbDockVisible", thumbDockVisibleAction->isChecked());

    /* Property Editor */
    setting->setValue("isSoloPrefDlg", isSoloPrefDlg);

     /* FolderDock floating info */
    setting->beginGroup(("FolderDock"));
    setting->setValue("screen", folderDock->dw.screen);
    setting->setValue("pos", folderDock->dw.pos);
    setting->setValue("size", folderDock->dw.size);
    setting->setValue("devicePixelRatio", folderDock->dw.devicePixelRatio);
    setting->endGroup();

    /* FavDock floating info */
    setting->beginGroup(("FavDock"));
    setting->setValue("screen", favDock->dw.screen);
    setting->setValue("pos", favDock->dw.pos);
    setting->setValue("size", favDock->dw.size);
    setting->setValue("devicePixelRatio", favDock->dw.devicePixelRatio);
    setting->endGroup();

    /* MetadataDock floating info */
//    if (G::useInfoView) {
        setting->beginGroup(("MetadataDock"));
        setting->setValue("screen", metadataDock->dw.screen);
        setting->setValue("pos", metadataDock->dw.pos);
        setting->setValue("size", metadataDock->dw.size);
        setting->setValue("devicePixelRatio", metadataDock->dw.devicePixelRatio);
        setting->endGroup();
//    }

    /* EmbelDock floating info */
    setting->beginGroup(("EmbelDock"));
    setting->setValue("screen", embelDock->dw.screen);
    setting->setValue("pos", embelDock->dw.pos);
    setting->setValue("size", embelDock->dw.size);
    setting->setValue("devicePixelRatio", embelDock->dw.devicePixelRatio);
    setting->endGroup();

    /* FilterDock floating info */
    setting->beginGroup(("FilterDock"));
    setting->setValue("screen", filterDock->dw.screen);
    setting->setValue("pos", filterDock->dw.pos);
    setting->setValue("size", filterDock->dw.size);
    setting->setValue("devicePixelRatio", filterDock->dw.devicePixelRatio);
    setting->endGroup();

    /* ThumbDock floating info */
    setting->beginGroup(("ThumbDockFloat"));
    setting->setValue("screen", thumbDock->dw.screen);
    setting->setValue("pos", thumbDock->dw.pos);
    setting->setValue("size", thumbDock->dw.size);
    setting->setValue("devicePixelRatio", thumbDock->dw.devicePixelRatio);
    setting->endGroup();

    /* InfoView okToShow fields */
    if (G::useInfoView) {
        setting->beginGroup("InfoFields");
        setting->remove("");
        QStandardItemModel *k = infoView->ok;
        for(int row = 0; row < k->rowCount(); row++) {
            QModelIndex parentIdx = k->index(row, 0);
            QString field = k->index(row, 0).data().toString();
            bool showField = k->index(row, 2).data().toBool();
            setting->setValue(field, showField);
            for (int childRow = 0; childRow < k->rowCount(parentIdx); childRow++) {
                QString field = k->index(childRow, 0, parentIdx).data().toString();
                bool showField = k->index(childRow, 2, parentIdx).data().toBool();
                setting->setValue(field, showField);
            }
        }
        setting->endGroup();
    }

    /* TableView okToShow fields */
    setting->beginGroup("TableFields");
    setting->remove("");
    for(int row = 0; row < tableView->ok->rowCount(); row++) {
        QString field = tableView->ok->index(row, 0).data().toString();
        bool showField = tableView->ok->index(row, 1).data().toBool();
        setting->setValue(field, showField);
    }
    setting->endGroup();

    /* Tokens used for ingest operations */
    setting->beginGroup("PathTokens");
    setting->remove("");
    // save path templates
    QMapIterator<QString, QString> pathIter(pathTemplates);
    while (pathIter.hasNext()) {
        pathIter.next();
        setting->setValue(pathIter.key(), pathIter.value());
    }
    setting->endGroup();

    // save filename templates
    setting->beginGroup("FileNameTokens");
    setting->remove("");
    QMapIterator<QString, QString> filenameIter(filenameTemplates);
    while (filenameIter.hasNext()) {
        filenameIter.next();
        setting->setValue(filenameIter.key(), filenameIter.value());
    }
    setting->endGroup();

    /* Token templates used for shooting information shown in ImageView */
    setting->setValue("loupeInfoTemplate", infoString->loupeInfoTemplate);
    setting->beginGroup("InfoTemplates");
        setting->remove("");
        QMapIterator<QString, QString> infoIter(infoString->infoTemplates);
        while (infoIter.hasNext()) {
            infoIter.next();
            setting->setValue(infoIter.key(), infoIter.value());
        }
    setting->endGroup();

    /* External apps */
//    setting->beginGroup("ExternalApps");
//    setting->remove("");
//    for (int i = 0; i < externalApps.length(); ++i) {
//        QString sortPrefix = xAppShortcut[i];
//        if (sortPrefix == "0") sortPrefix = "X";
//        setting->setValue(sortPrefix + externalApps.at(i).name, externalApps.at(i).path);
//    }
//    setting->endGroup();

    /* save bookmarks */
    int idx = 0;
    setting->beginGroup("Bookmarks");
    setting->remove("");
    QSetIterator<QString> pathsIter(bookmarks->bookmarkPaths);
    while (pathsIter.hasNext()) {
        setting->setValue("path" + QString::number(++idx), pathsIter.next());
    }
    setting->endGroup();

    /* save recent folders */
    setting->beginGroup("RecentFolders");
    setting->remove("");
    QString leadingZero;
    for (int i = 0; i < recentFolders->count(); i++) {
        i < 9 ? leadingZero = "0" : leadingZero = "";
        setting->setValue("recentFolder" + leadingZero + QString::number(i+1),
                          recentFolders->at(i));
    }
    setting->endGroup();

    // moved to ingest()
//    /* save ingest history folders */
//    setting->beginGroup("IngestHistoryFolders");
//    setting->remove("");
//    for (int i = 0; i < ingestHistoryFolders->count(); i++) {
//        setting->setValue("ingestHistoryFolder" + QString::number(i+1),
//                          ingestHistoryFolders->at(i));
//    }
//    setting->endGroup();

//    /* save ingest description completer list */
//    setting->beginGroup("IngestDescriptionCompleter");
//    for (const auto& i : ingestDescriptionCompleter) {
//        setting->setValue(i, "");
//    }
//    setting->endGroup();

    saveWorkspaces();
}

bool MW::loadSettings()
{
/*
    Persistant settings from QSettings fall into two categories:
    1.  Action settings
    2.  Preferences

    Not all settings are loaded here in this function, since this function is called before the
    actions and many object, such as imageView, are created.

    Preferences are located in the prefdlg class and updated here.
*/
    if (G::isLogger) G::log("MW::loadSettings");

    /* Default values for first time use (settings does not yet exist).  Apply defaults even
       when there are settings in case a new setting has been added to the application to make
       sure the default value is assigned for first time use of the new setting.
    */

    if (!isSettings || simulateJustInstalled) {
        // general
        combineRawJpg = true;
        prevMode = "Loupe";
        G::mode = "Loupe";
        G::isLogger = false;
        G::isFileLogger = false;
        G::isErrorLogger = false;
        G::wheelSensitivity = 40;
        G::modifySourceFiles = false;
        G::backupBeforeModifying = false;
        G::autoAddMissingThumbnails = false;
        ignoreAddThumbnailsDlg = false;

        // appearance
        G::backgroundShade = 50;
        G::strFontSize = "12";
        infoOverlayFontSize = 24;
        classificationBadgeInImageDiameter = 32;
        classificationBadgeSizeFactor = 14;
        iconNumberSize = 24;

        isRatingBadgeVisible = false;
        isIconNumberVisible = true;

        // datamodel
        G::maxIconSize = 256;
        sortColumn = G::NameColumn;
        dm->showThumbNailSymbolHelp = true;

        // files
        G::colorManage = true;
        rememberLastDir = false;
        checkIfUpdate = true;
        lastDir = "";
        deleteWarning = true;
        G::modifySourceFiles = false;
        G::modifySourceFiles = false;
        G::useSidecar = false;
//        G::embedTifJpgThumb = false;

        // ingest
        autoIngestFolderPath = false;
        autoEjectUsb = false;
        integrityCheck = false;
        isBackgroundIngest = false;
        isBackgroundIngestBeep = false;
        ingestIncludeXmpSidecar = true;
        gotoIngestFolder = false;
        backupIngest = false;
        pathTemplateSelected = 0;
        pathTemplateSelected2 = 0;
        filenameTemplateSelected = 0;

        // preferences
        isSoloPrefDlg = true;

        // slideshow
        slideShowDelay = 5;
        isSlideShowRandom = false;
        isSlideShowWrap = true;

        // filters

        // cache
        G::isLoadLinear = false;
        G::isLoadConcurrent = true;
        cacheMethod = "Concurrent";
        cacheSizeStrategy = "Moderate";
        cacheMinSize = "  2 GB";
        cacheSizePercentOfAvailable = 50;
        cacheMaxMB = static_cast<int>(G::availableMemoryMB * 0.5);
        isShowCacheProgressBar = true;
        progressWidth = 200;
        progressWidthBeforeResizeWindow = progressWidth;
        cacheWtAhead = 7;
        isCachePreview = false;
        cachePreviewWidth = 2000;
        cachePreviewHeight = 1600;

        if (!isSettings || simulateJustInstalled) return true;
    }
    // end default settings

    // Get settings saved from last session

    // general
//    sortColumn = setting->value("sortColumn").toInt();
//    prevSortColumn = sortColumn;
//    isReverseSort = setting->value("sortReverse").toBool();
    autoAdvance = setting->value("autoAdvance").toBool();
    turnOffEmbellish = setting->value("turnOffEmbellish").toBool();
    /*
    if (setting->contains("isFileLogger"))
        G::isFileLogger = setting->value("isFileLogger").toBool();
    else
        G::isFileLogger = false;
    if (setting->contains("isErrorLogger"))
        G::isErrorLogger = setting->value("isErrorLogger").toBool();
    else
        G::isErrorLogger = false;
    if (G::isFileLogger || G::isErrorLogger)
        G::isLogger = true;
    else
        G::isLogger = false;
    */
    if (setting->contains("deleteWarning"))
        deleteWarning = setting->value("deleteWarning").toBool();
    else
        deleteWarning = true;

    if (setting->contains("modifySourceFiles"))
        G::modifySourceFiles = setting->value("modifySourceFiles").toBool();
    else
        G::modifySourceFiles = false;

    if (setting->contains("backupBeforeModifying"))
        G::backupBeforeModifying = setting->value("backupBeforeModifying").toBool();
    else
        G::backupBeforeModifying = false;

    if (setting->contains("autoAddMissingThumbnails"))
        G::autoAddMissingThumbnails = setting->value("autoAddMissingThumbnails").toBool();
    else
        G::autoAddMissingThumbnails = false;

    if (setting->contains("ignoreAddThumbnailsDlg"))
        ignoreAddThumbnailsDlg = setting->value("ignoreAddThumbnailsDlg").toBool();
    else
        ignoreAddThumbnailsDlg = false;

    if (setting->contains("useSidecar"))
        G::useSidecar = setting->value("useSidecar").toBool();
    else
        G::useSidecar = false;

    if (setting->contains("renderVideoThumb"))
        G::renderVideoThumb = setting->value("renderVideoThumb").toBool();
    else
        G::renderVideoThumb = false;

    if (setting->contains("wheelSensitivity"))
        G::wheelSensitivity = setting->value("wheelSensitivity").toInt();
    else
        G::wheelSensitivity = 40;

    lastFileIfCrash = setting->value("lastFileSelection").toString();

    // appearance
    if (setting->contains("backgroundShade")) {
        G::backgroundShade = setting->value("backgroundShade").toInt();
        if (G::backgroundShade < 20) G::backgroundShade = 50;
        G::backgroundColor = QColor(G::backgroundShade,G::backgroundShade,G::backgroundShade);
    }
    if (setting->contains("fontSize")) {
        G::strFontSize = setting->value("fontSize").toString();
        if (G::strFontSize == "") G::strFontSize = "12";
        G::fontSize = G::strFontSize.toInt();
    }

    // thumbdock
    if (setting->contains("wasThumbDockVisible")) wasThumbDockVisible = setting->value("wasThumbDockVisible").toBool();

    // load imageView->infoOverlayFontSize later as imageView has not been created yet
    if (setting->contains("classificationBadgeInImageDiameter")) classificationBadgeInImageDiameter = setting->value("classificationBadgeInImageDiameter").toInt();
    if (setting->contains("classificationBadgeSizeFactor")) classificationBadgeSizeFactor = setting->value("classificationBadgeSizeFactor").toInt();
    else classificationBadgeSizeFactor = 14;

    if (setting->contains("iconNumberSize")) iconNumberSize = setting->value("iconNumberSize").toInt();
    else iconNumberSize = 26;

    if (setting->contains("isRatingBadgeVisible")) isRatingBadgeVisible = setting->value("isRatingBadgeVisible").toBool();
    if (setting->contains("isIconNumberVisible")) isIconNumberVisible = setting->value("isIconNumberVisible").toBool();
    else isIconNumberVisible = true;

    // datamodel
    if (setting->contains("maxIconSize")) G::maxIconSize = setting->value("maxIconSize").toInt();

    // files
    if (setting->contains("colorManage")) G::colorManage = setting->value("colorManage").toBool();
    if (setting->contains("rememberLastDir")) rememberLastDir = setting->value("rememberLastDir").toBool();
    if (setting->contains("checkIfUpdate")) checkIfUpdate = setting->value("checkIfUpdate").toBool();
    if (setting->contains("lastDir")) lastDir = setting->value("lastDir").toString();

    // ingest
    if (setting->contains("autoIngestFolderPath")) autoIngestFolderPath = setting->value("autoIngestFolderPath").toBool();
    if (setting->contains("autoEjectUSB")) autoEjectUsb = setting->value("autoEjectUSB").toBool();
    if (setting->contains("integrityCheck")) integrityCheck = setting->value("integrityCheck").toBool();
    if (setting->contains("isBackgroundIngest")) isBackgroundIngest = setting->value("isBackgroundIngest").toBool();
    if (setting->contains("isBackgroundIngestBeep")) isBackgroundIngestBeep = setting->value("isBackgroundIngestBeep").toBool();
    if (setting->contains("ingestIncludeXmpSidecar")) ingestIncludeXmpSidecar = setting->value("ingestIncludeXmpSidecar").toBool();
    if (setting->contains("backupIngest")) backupIngest = setting->value("backupIngest").toBool();
    if (setting->contains("gotoIngestFolder")) gotoIngestFolder = setting->value("gotoIngestFolder").toBool();
    if (setting->contains("ingestRootFolder")) ingestRootFolder = setting->value("ingestRootFolder").toString();
    if (setting->contains("ingestRootFolder2")) ingestRootFolder2 = setting->value("ingestRootFolder2").toString();
    if (setting->contains("pathTemplateSelected")) pathTemplateSelected = setting->value("pathTemplateSelected").toInt();
    if (setting->contains("pathTemplateSelected2")) pathTemplateSelected2 = setting->value("pathTemplateSelected2").toInt();
    if (setting->contains("filenameTemplateSelected")) filenameTemplateSelected = setting->value("filenameTemplateSelected").toInt();
    if (setting->contains("manualFolderPath")) manualFolderPath = setting->value("manualFolderPath").toString();
    if (setting->contains("manualFolderPath2")) manualFolderPath2 = setting->value("manualFolderPath2").toString();
    if (setting->contains("ingestCount")) G::ingestCount = setting->value("ingestCount").toInt();
    if (setting->contains("ingestLastSeqDate")) G::ingestLastSeqDate = setting->value("ingestLastSeqDate").toDate();
    if (G::ingestLastSeqDate != QDate::currentDate()) G::ingestCount = 0;

    // preferences
    if (setting->contains("isSoloPrefDlg")) isSoloPrefDlg = setting->value("isSoloPrefDlg").toBool();

    // slideshow
    if (setting->contains("slideShowDelay")) slideShowDelay = setting->value("slideShowDelay").toInt();
    if (setting->contains("isSlideShowRandom")) isSlideShowRandom = setting->value("isSlideShowRandom").toBool();
    if (setting->contains("isSlideShowWrap")) isSlideShowWrap = setting->value("isSlideShowWrap").toBool();

    // metadata and icon cache loaded when metadataCacheThread created in MW::createCaching

    // image cache
    if (setting->contains("cacheSizePercentOfAvailable")) cacheSizePercentOfAvailable = setting->value("cacheSizePercentOfAvailable").toInt();
    if (setting->contains("cacheSizePercentOfAvailable")) cacheSizePercentOfAvailable = setting->value("cacheSizePercentOfAvailable").toInt();
    if (setting->contains("cacheMethod")) setCacheMethod(setting->value("cacheMethod").toString());
    else setCacheMethod("Concurrent");
    if (setting->contains("cacheSizeMethod")) setImageCacheSize(setting->value("cacheSizeMethod").toString());
    else setImageCacheSize("Moderate");
    if (setting->contains("cacheMinSize")) setImageCacheMinSize(setting->value("cacheMinSize").toString());
    else setImageCacheMinSize("4 GB");
    if (setting->contains("isShowCacheStatus")) isShowCacheProgressBar = setting->value("isShowCacheStatus").toBool();
    if (setting->contains("cacheStatusWidth")) progressWidth = setting->value("cacheStatusWidth").toInt();
    progressWidthBeforeResizeWindow = progressWidth;
    if (setting->contains("cacheWtAhead")) cacheWtAhead = setting->value("cacheWtAhead").toInt();
    if (setting->contains("isCachePreview")) isCachePreview = setting->value("isCachePreview").toBool();
    if (setting->contains("cachePreviewWidth")) cachePreviewWidth = setting->value("cachePreviewWidth").toInt();
    if (setting->contains("cachePreviewHeight")) cachePreviewHeight = setting->value("cachePreviewHeight").toInt();

    // full screen
    if (setting->contains("isFullScreenFolders")) fullScreenDocks.isFolders = setting->value("isFullScreenFolders").toBool();
    if (setting->contains("isFullScreenFavs")) fullScreenDocks.isFavs = setting->value("isFullScreenFavs").toBool();
    if (setting->contains("isFullScreenFilters")) fullScreenDocks.isFilters = setting->value("isFullScreenFilters").toBool();
    if (setting->contains("isFullScreenMetadata")) fullScreenDocks.isMetadata = setting->value("isFullScreenMetadata").toBool();
    if (setting->contains("isFullScreenThumbs")) fullScreenDocks.isThumbs = setting->value("isFullScreenThumbs").toBool();
    if (setting->contains("isFullScreenStatusBar")) fullScreenDocks.isStatusBar = setting->value("isFullScreenStatusBar").toBool();


    /* read external apps */
    /* moved to createActions as required to populate open with ... menu */

    /* read ingest token templates */
    setting->beginGroup("PathTokens");
    QStringList keys = setting->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        pathTemplates[key] = setting->value(key).toString();
    }
    setting->endGroup();

    setting->beginGroup("FileNameTokens");
    keys = setting->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        filenameTemplates[key] = setting->value(key).toString();
        //qDebug() << "Save FileNameTokens" << key;
    }
    setting->endGroup();

    /* read recent folders */
    setting->beginGroup("RecentFolders");
    QStringList recentList = setting->childKeys();
    for (int i = 0; i < recentList.size(); ++i) {
        recentFolders->append(setting->value(recentList.at(i)).toString());
    }
    setting->endGroup();

    /* read ingest history folders */
    setting->beginGroup("IngestHistoryFolders");
    QStringList ingestHistoryList = setting->childKeys();
    for (int i = 0; i < ingestHistoryList.size(); ++i) {
        ingestHistoryFolders->append(setting->value(ingestHistoryList.at(i)).toString());
     }
    setting->endGroup();

    /* read ingest description completer list
       Can use a simpler format to read the list (compared to read recent folders)
       because there will not be any "/" or "\" characters which the windows registry
       does not like.
    */
    setting->beginGroup("IngestDescriptionCompleter");
    ingestDescriptionCompleter = setting->childKeys();
    setting->endGroup();

    loadWorkspaces();
    // moved read workspaces to separate function as req'd by actions while the
    // rest of QSettings are dependent on actions being defined first.

    return true;
}

void MW::removeDeprecatedSettings()
{
    if (G::isLogger) G::log("MW::removeDeprecatedSettings");

    QStringList deprecatedSettings;
    deprecatedSettings
        << "tryConcurrentLoading"
           ;

}
