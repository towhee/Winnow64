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
    settings->setValue("lastPrefPage", lastPrefPage);
//    setting->setValue("mouseClickScroll", mouseClickScroll);
    settings->setValue("toggleZoomValue", imageView->toggleZoom);
    settings->setValue("limitFit100Pct", imageView->limitFit100Pct);
//    setting->setValue("sortColumn", sortColumn);
//    setting->setValue("sortReverse", sortReverseAction->isChecked());
    settings->setValue("autoAdvance", autoAdvance);
    settings->setValue("turnOffEmbellish", turnOffEmbellish);
    settings->setValue("deleteWarning", deleteWarning);
    settings->setValue("modifySourceFiles", G::modifySourceFiles);
    settings->setValue("backupBeforeModifying", G::backupBeforeModifying);
    settings->setValue("autoAddMissingThumbnails", G::autoAddMissingThumbnails);
    settings->setValue("ignoreAddThumbnailsDlg", ignoreAddThumbnailsDlg);
    settings->setValue("useSidecar", G::useSidecar);
//    setting->setValue("embedTifThumb", G::embedTifJpgThumb);
    settings->setValue("renderVideoThumb", G::renderVideoThumb);
    settings->setValue("isLogger", G::isLogger);
    settings->setValue("isErrorLogger", G::isErrorLogger);
    settings->setValue("wheelSensitivity", G::wheelSensitivity);

    // datamodel
    settings->setValue("maxIconSize", G::maxIconSize);

    // appearance
    settings->setValue("backgroundShade", G::backgroundShade);
    settings->setValue("fontSize", G::strFontSize);
    settings->setValue("classificationBadgeInImageDiameter", classificationBadgeInImageDiameter);
    settings->setValue("classificationBadgeSizeFactor", classificationBadgeSizeFactor);
    settings->setValue("iconNumberSize", iconNumberSize);
    settings->setValue("infoOverlayFontSize", imageView->infoOverlayFontSize);

    // files
    settings->setValue("colorManage", G::colorManage);
    settings->setValue("rememberLastDir", rememberLastDir);
    settings->setValue("checkIfUpdate", checkIfUpdate);
//    setting->setValue("includeSubfolders", subFoldersAction->isChecked());
    settings->setValue("combineRawJpg", combineRawJpg);

    /* ingest (moved to MW::ingest)
    */

    // thumbs
    settings->setValue("thumbWidth", thumbView->iconWidth);
    settings->setValue("thumbHeight", thumbView->iconHeight);
    settings->setValue("labelFontSize", thumbView->labelFontSize);
    settings->setValue("showThumbLabels", thumbView->showIconLabels);
    settings->setValue("showZoomFrame", thumbView->showZoomFrame);

    // grid
    settings->setValue("thumbWidthGrid", gridView->iconWidth);
    settings->setValue("thumbHeightGrid", gridView->iconHeight);
    settings->setValue("labelFontSizeGrid", gridView->labelFontSize);
    settings->setValue("showThumbLabelsGrid", gridView->showIconLabels);
    settings->setValue("labelChoice", gridView->labelChoice);

    settings->setValue("thumbsPerPage", metadataCacheThread->visibleIcons);

    // slideshow
    settings->setValue("slideShowDelay", slideShowDelay);
    settings->setValue("isSlideShowRandom", isSlideShowRandom);
    settings->setValue("isSlideShowWrap", isSlideShowWrap);

    // metadata and icon cache
    settings->setValue("cacheAllMetadata", metadataCacheThread->cacheAllMetadata);
    settings->setValue("cacheAllIcons", metadataCacheThread->cacheAllIcons);
//    setting->setValue("iconChunkSize", dm->iconChunkSize);

    // image cache
    settings->setValue("cacheMethod", cacheMethod);
    settings->setValue("cacheSizeMethod", cacheSizeStrategy);
    settings->setValue("cacheMinSize", cacheMinSize);
    settings->setValue("isShowCacheStatus", isShowCacheProgressBar);
    settings->setValue("cacheDelay", cacheDelay);
    settings->setValue("cacheStatusWidth", progressWidth);
    settings->setValue("cacheWtAhead", cacheWtAhead);
    settings->setValue("isCachePreview", isCachePreview);
    settings->setValue("cachePreviewWidth", cachePreviewWidth);
    settings->setValue("cachePreviewHeight", cachePreviewHeight);

    // full screen
    settings->setValue("isFullScreenFolders", fullScreenDocks.isFolders);
    settings->setValue("isFullScreenFavs", fullScreenDocks.isFavs);
    settings->setValue("isFullScreenFilters", fullScreenDocks.isFilters);
    settings->setValue("isFullScreenMetadata", fullScreenDocks.isMetadata);
    settings->setValue("isFullScreenThumbs", fullScreenDocks.isThumbs);
    settings->setValue("isFullScreenStatusBar", fullScreenDocks.isStatusBar);

    // state
    settings->setValue("Geometry", saveGeometry());
    settings->setValue("WindowState", saveState());
    settings->setValue("WindowLocation", geometry());
    settings->setValue("isFullScreen", isFullScreen());

    settings->setValue("isRatingBadgeVisible", ratingBadgeVisibleAction->isChecked());
    settings->setValue("isIconNumberVisible", iconNumberVisibleAction->isChecked());
    settings->setValue("isImageInfoVisible", infoVisibleAction->isChecked());
    settings->setValue("isLoupeDisplay", asLoupeAction->isChecked());
    settings->setValue("isGridDisplay", asGridAction->isChecked());
    settings->setValue("isTableDisplay", asTableAction->isChecked());
    settings->setValue("isCompareDisplay", asCompareAction->isChecked());

    settings->setValue("wasThumbDockVisible", wasThumbDockVisible);

    settings->setValue("isMenuBarVisible", menuBarVisibleAction->isChecked());
    settings->setValue("isStatusBarVisible", statusBarVisibleAction->isChecked());
    settings->setValue("isFolderDockVisible", folderDockVisibleAction->isChecked());
    settings->setValue("isFavDockVisible", favDockVisibleAction->isChecked());
    settings->setValue("isFilterDockVisible", filterDockVisibleAction->isChecked());
    settings->setValue("isMetadataDockVisible", metadataDockVisibleAction->isChecked());
    settings->setValue("isEmbelDockVisible", embelDockVisibleAction->isChecked());
    settings->setValue("isThumbDockVisible", thumbDockVisibleAction->isChecked());

    /* Property Editor */
    settings->setValue("isSoloPrefDlg", isSoloPrefDlg);

     /* FolderDock floating info */
    settings->beginGroup(("FolderDock"));
    settings->setValue("screen", folderDock->dw.screen);
    settings->setValue("pos", folderDock->dw.pos);
    settings->setValue("size", folderDock->dw.size);
    settings->setValue("devicePixelRatio", folderDock->dw.devicePixelRatio);
    settings->endGroup();

    /* FavDock floating info */
    settings->beginGroup(("FavDock"));
    settings->setValue("screen", favDock->dw.screen);
    settings->setValue("pos", favDock->dw.pos);
    settings->setValue("size", favDock->dw.size);
    settings->setValue("devicePixelRatio", favDock->dw.devicePixelRatio);
    settings->endGroup();

    /* MetadataDock floating info */
//    if (G::useInfoView) {
    settings->beginGroup(("MetadataDock"));
    settings->setValue("screen", metadataDock->dw.screen);
    settings->setValue("pos", metadataDock->dw.pos);
    settings->setValue("size", metadataDock->dw.size);
    settings->setValue("devicePixelRatio", metadataDock->dw.devicePixelRatio);
    settings->endGroup();
//    }

    /* EmbelDock floating info */
    settings->beginGroup(("EmbelDock"));
    settings->setValue("screen", embelDock->dw.screen);
    settings->setValue("pos", embelDock->dw.pos);
    settings->setValue("size", embelDock->dw.size);
    settings->setValue("devicePixelRatio", embelDock->dw.devicePixelRatio);
    settings->endGroup();

    /* FilterDock floating info */
    settings->beginGroup(("FilterDock"));
    settings->setValue("screen", filterDock->dw.screen);
    settings->setValue("pos", filterDock->dw.pos);
    settings->setValue("size", filterDock->dw.size);
    settings->setValue("devicePixelRatio", filterDock->dw.devicePixelRatio);
    settings->endGroup();

    /* ThumbDock floating info */
    settings->beginGroup(("ThumbDockFloat"));
    settings->setValue("screen", thumbDock->dw.screen);
    settings->setValue("pos", thumbDock->dw.pos);
    settings->setValue("size", thumbDock->dw.size);
    settings->setValue("devicePixelRatio", thumbDock->dw.devicePixelRatio);
    settings->endGroup();

    /* InfoView okToShow fields */
    if (G::useInfoView) {
        settings->beginGroup("InfoFields");
        settings->remove("");
        QStandardItemModel *k = infoView->ok;
        for(int row = 0; row < k->rowCount(); row++) {
            QModelIndex parentIdx = k->index(row, 0);
            QString field = k->index(row, 0).data().toString();
            bool showField = k->index(row, 2).data().toBool();
            settings->setValue(field, showField);
            for (int childRow = 0; childRow < k->rowCount(parentIdx); childRow++) {
                QString field = k->index(childRow, 0, parentIdx).data().toString();
                bool showField = k->index(childRow, 2, parentIdx).data().toBool();
                settings->setValue(field, showField);
            }
        }
        settings->endGroup();
    }

    /* TableView okToShow fields */
    settings->beginGroup("TableFields");
    settings->remove("");
    for(int row = 0; row < tableView->ok->rowCount(); row++) {
        QString field = tableView->ok->index(row, 0).data().toString();
        bool showField = tableView->ok->index(row, 1).data().toBool();
        settings->setValue(field, showField);
    }
    settings->endGroup();

    /* Tokens used for ingest operations */
    settings->beginGroup("PathTokens");
    settings->remove("");
    // save path templates
    QMapIterator<QString, QString> pathIter(pathTemplates);
    while (pathIter.hasNext()) {
        pathIter.next();
        settings->setValue(pathIter.key(), pathIter.value());
    }
    settings->endGroup();

    // save filename templates
    settings->beginGroup("FileNameTokens");
    settings->remove("");
    QMapIterator<QString, QString> filenameIter(filenameTemplates);
    while (filenameIter.hasNext()) {
        filenameIter.next();
        settings->setValue(filenameIter.key(), filenameIter.value());
    }
    settings->endGroup();

    /* Token templates used for shooting information shown in ImageView */
    settings->setValue("loupeInfoTemplate", infoString->loupeInfoTemplate);
    settings->beginGroup("InfoTemplates");
    settings->remove("");
        QMapIterator<QString, QString> infoIter(infoString->infoTemplates);
        while (infoIter.hasNext()) {
            infoIter.next();
        settings->setValue(infoIter.key(), infoIter.value());
        }
        settings->endGroup();

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
        settings->beginGroup("Bookmarks");
    settings->remove("");
    QSetIterator<QString> pathsIter(bookmarks->bookmarkPaths);
    while (pathsIter.hasNext()) {
        settings->setValue("path" + QString::number(++idx), pathsIter.next());
    }
    settings->endGroup();

//    /* save recent folders */
//    setting->beginGroup("RecentFolders");
//    setting->remove("");
//    QString leadingZero;
//    for (int i = 0; i < recentFolders->count(); i++) {
//        i < 9 ? leadingZero = "0" : leadingZero = "";
//        setting->setValue("recentFolder" + leadingZero + QString::number(i+1),
//                          recentFolders->at(i));
//    }
//    setting->endGroup();

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

//    saveWorkspaces();
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
        G::backgroundColor = QColor(G::backgroundShade,G::backgroundShade,G::backgroundShade);
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
        cacheMinMB = 2000;
        cacheMinSize = "2 GB";
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
    autoAdvance = settings->value("autoAdvance").toBool();
    turnOffEmbellish = settings->value("turnOffEmbellish").toBool();
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
    if (settings->contains("deleteWarning"))
        deleteWarning = settings->value("deleteWarning").toBool();
    else
        deleteWarning = true;

    if (settings->contains("modifySourceFiles"))
        G::modifySourceFiles = settings->value("modifySourceFiles").toBool();
    else
        G::modifySourceFiles = false;

    if (settings->contains("backupBeforeModifying"))
        G::backupBeforeModifying = settings->value("backupBeforeModifying").toBool();
    else
        G::backupBeforeModifying = false;

    if (settings->contains("autoAddMissingThumbnails"))
        G::autoAddMissingThumbnails = settings->value("autoAddMissingThumbnails").toBool();
    else
        G::autoAddMissingThumbnails = false;

    if (settings->contains("ignoreAddThumbnailsDlg"))
        ignoreAddThumbnailsDlg = settings->value("ignoreAddThumbnailsDlg").toBool();
    else
        ignoreAddThumbnailsDlg = false;

    if (settings->contains("useSidecar"))
        G::useSidecar = settings->value("useSidecar").toBool();
    else
        G::useSidecar = false;

    if (settings->contains("renderVideoThumb"))
        G::renderVideoThumb = settings->value("renderVideoThumb").toBool();
    else
        G::renderVideoThumb = false;

    if (settings->contains("wheelSensitivity"))
        G::wheelSensitivity = settings->value("wheelSensitivity").toInt();
    else
        G::wheelSensitivity = 40;

    lastFileIfCrash = settings->value("lastFileSelection").toString();

    // appearance
    if (settings->contains("backgroundShade")) {
        G::backgroundShade = settings->value("backgroundShade").toInt();
        if (G::backgroundShade < 20) G::backgroundShade = 50;
        G::backgroundColor = QColor(G::backgroundShade,G::backgroundShade,G::backgroundShade);
    }
    if (settings->contains("fontSize")) {
        G::strFontSize = settings->value("fontSize").toString();
        if (G::strFontSize == "") G::strFontSize = "12";
        G::fontSize = G::strFontSize.toInt();
    }

    // thumbdock
    if (settings->contains("wasThumbDockVisible")) wasThumbDockVisible = settings->value("wasThumbDockVisible").toBool();

    // load imageView->infoOverlayFontSize later as imageView has not been created yet
    if (settings->contains("classificationBadgeInImageDiameter")) classificationBadgeInImageDiameter = settings->value("classificationBadgeInImageDiameter").toInt();
    if (settings->contains("classificationBadgeSizeFactor")) classificationBadgeSizeFactor = settings->value("classificationBadgeSizeFactor").toInt();
    else classificationBadgeSizeFactor = 14;

    if (settings->contains("iconNumberSize")) iconNumberSize = settings->value("iconNumberSize").toInt();
    else iconNumberSize = 26;

    if (settings->contains("isRatingBadgeVisible")) isRatingBadgeVisible = settings->value("isRatingBadgeVisible").toBool();
    if (settings->contains("isIconNumberVisible")) isIconNumberVisible = settings->value("isIconNumberVisible").toBool();
    else isIconNumberVisible = true;

    // datamodel
    if (settings->contains("maxIconSize")) G::maxIconSize = settings->value("maxIconSize").toInt();

    // files
    if (settings->contains("colorManage")) G::colorManage = settings->value("colorManage").toBool();
    if (settings->contains("rememberLastDir")) rememberLastDir = settings->value("rememberLastDir").toBool();
    if (settings->contains("checkIfUpdate")) checkIfUpdate = settings->value("checkIfUpdate").toBool();
    if (settings->contains("lastDir")) lastDir = settings->value("lastDir").toString();

    // ingest
    if (settings->contains("autoIngestFolderPath")) autoIngestFolderPath = settings->value("autoIngestFolderPath").toBool();
    if (settings->contains("autoEjectUSB")) autoEjectUsb = settings->value("autoEjectUSB").toBool();
    if (settings->contains("integrityCheck")) integrityCheck = settings->value("integrityCheck").toBool();
    if (settings->contains("isBackgroundIngest")) isBackgroundIngest = settings->value("isBackgroundIngest").toBool();
    if (settings->contains("isBackgroundIngestBeep")) isBackgroundIngestBeep = settings->value("isBackgroundIngestBeep").toBool();
    if (settings->contains("ingestIncludeXmpSidecar")) ingestIncludeXmpSidecar = settings->value("ingestIncludeXmpSidecar").toBool();
    if (settings->contains("backupIngest")) backupIngest = settings->value("backupIngest").toBool();
    if (settings->contains("gotoIngestFolder")) gotoIngestFolder = settings->value("gotoIngestFolder").toBool();
    if (settings->contains("ingestRootFolder")) ingestRootFolder = settings->value("ingestRootFolder").toString();
    if (settings->contains("ingestRootFolder2")) ingestRootFolder2 = settings->value("ingestRootFolder2").toString();
    if (settings->contains("pathTemplateSelected")) pathTemplateSelected = settings->value("pathTemplateSelected").toInt();
    if (settings->contains("pathTemplateSelected2")) pathTemplateSelected2 = settings->value("pathTemplateSelected2").toInt();
    if (settings->contains("filenameTemplateSelected")) filenameTemplateSelected = settings->value("filenameTemplateSelected").toInt();
    if (settings->contains("manualFolderPath")) manualFolderPath = settings->value("manualFolderPath").toString();
    if (settings->contains("manualFolderPath2")) manualFolderPath2 = settings->value("manualFolderPath2").toString();
    if (settings->contains("ingestCount")) G::ingestCount = settings->value("ingestCount").toInt();
    if (settings->contains("ingestLastSeqDate")) G::ingestLastSeqDate = settings->value("ingestLastSeqDate").toDate();
    if (G::ingestLastSeqDate != QDate::currentDate()) G::ingestCount = 0;

    // preferences
    if (settings->contains("isSoloPrefDlg")) isSoloPrefDlg = settings->value("isSoloPrefDlg").toBool();

    // slideshow
    if (settings->contains("slideShowDelay")) slideShowDelay = settings->value("slideShowDelay").toInt();
    if (settings->contains("isSlideShowRandom")) isSlideShowRandom = settings->value("isSlideShowRandom").toBool();
    if (settings->contains("isSlideShowWrap")) isSlideShowWrap = settings->value("isSlideShowWrap").toBool();

    // metadata and icon cache loaded when metadataCacheThread created in MW::createCaching

    // image cache
    if (settings->contains("cacheSizePercentOfAvailable")) cacheSizePercentOfAvailable = settings->value("cacheSizePercentOfAvailable").toInt();
    if (settings->contains("cacheSizePercentOfAvailable")) cacheSizePercentOfAvailable = settings->value("cacheSizePercentOfAvailable").toInt();
    if (settings->contains("cacheMethod")) setCacheMethod(settings->value("cacheMethod").toString());
    else setCacheMethod("Concurrent");
    if (settings->contains("cacheSizeMethod")) setImageCacheSize(settings->value("cacheSizeMethod").toString());
    else setImageCacheSize("Moderate");
    if (settings->contains("cacheMinSize")) setImageCacheMinSize(settings->value("cacheMinSize").toString());
    else setImageCacheMinSize("4 GB");
    if (settings->contains("isShowCacheStatus")) isShowCacheProgressBar = settings->value("isShowCacheStatus").toBool();
    if (settings->contains("cacheStatusWidth")) progressWidth = settings->value("cacheStatusWidth").toInt();
    progressWidthBeforeResizeWindow = progressWidth;
    if (settings->contains("cacheWtAhead")) cacheWtAhead = settings->value("cacheWtAhead").toInt();
    if (settings->contains("isCachePreview")) isCachePreview = settings->value("isCachePreview").toBool();
    if (settings->contains("cachePreviewWidth")) cachePreviewWidth = settings->value("cachePreviewWidth").toInt();
    if (settings->contains("cachePreviewHeight")) cachePreviewHeight = settings->value("cachePreviewHeight").toInt();

    // full screen
    if (settings->contains("isFullScreenFolders")) fullScreenDocks.isFolders = settings->value("isFullScreenFolders").toBool();
    if (settings->contains("isFullScreenFavs")) fullScreenDocks.isFavs = settings->value("isFullScreenFavs").toBool();
    if (settings->contains("isFullScreenFilters")) fullScreenDocks.isFilters = settings->value("isFullScreenFilters").toBool();
    if (settings->contains("isFullScreenMetadata")) fullScreenDocks.isMetadata = settings->value("isFullScreenMetadata").toBool();
    if (settings->contains("isFullScreenThumbs")) fullScreenDocks.isThumbs = settings->value("isFullScreenThumbs").toBool();
    if (settings->contains("isFullScreenStatusBar")) fullScreenDocks.isStatusBar = settings->value("isFullScreenStatusBar").toBool();


    /* read external apps */
    /* moved to createActions as required to populate open with ... menu */

    /* read ingest token templates */
    settings->beginGroup("PathTokens");
    QStringList keys = settings->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        pathTemplates[key] = settings->value(key).toString();
    }
    settings->endGroup();

    settings->beginGroup("FileNameTokens");
    keys = settings->childKeys();
    for (int i = 0; i < keys.size(); ++i) {
        QString key = keys.at(i);
        filenameTemplates[key] = settings->value(key).toString();
        //qDebug() << "Save FileNameTokens" << key;
    }
    settings->endGroup();

    /* read recent folders */
    settings->beginGroup("RecentFolders");
    QStringList recentList = settings->childKeys();
    for (int i = 0; i < recentList.size(); ++i) {
        recentFolders->append(settings->value(recentList.at(i)).toString());
    }
    settings->endGroup();

    /* read ingest history folders */
    settings->beginGroup("IngestHistoryFolders");
    QStringList ingestHistoryList = settings->childKeys();
    for (int i = 0; i < ingestHistoryList.size(); ++i) {
        ingestHistoryFolders->append(settings->value(ingestHistoryList.at(i)).toString());
     }
    settings->endGroup();

    /* read ingest description completer list
       Can use a simpler format to read the list (compared to read recent folders)
       because there will not be any "/" or "\" characters which the windows registry
       does not like.
    */
    settings->beginGroup("IngestDescriptionCompleter");
    ingestDescriptionCompleter = settings->childKeys();
    settings->endGroup();

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
