#include "Main/mainwindow.h"
#include "Cache/devpreviewcache.h"
#include "Cache/thumbcache.h"
#include "ImageFormats/Raw/applerawdecode.h"
#include "Utilities/appinstaller.h"

void MW::writeSetting(QString key, QVariant value)
{
    qDebug() << "MW::writeSetting" << key << value;
    settings->setValue(key, value);
}

void MW::writeSettings()
{
/*
    This function is called when exiting the application.

    Using QSetting, write the persistent application settings so they can be re-established
    when the application is re-opened.
*/
    if (G::isLogger) G::log("MW::writeSettings");

    // state
    settings->setValue("Geometry", saveGeometry());
    /* Versioned so a state written here is only restored by a build with the same dock set
       (see MW::winnowStateVersion and MW::showEvent). */
    settings->setValue("WindowState", saveState(winnowStateVersion));
    settings->setValue("isFullScreen", isFullScreen());
    /* The workflow the session is being left in.  The next session starts in Preview,
       so a Develop layout must not be the one it comes up in (see MW::showEvent). */
    settings->setValue("currentWorkflow", currentWorkflow);

    /*  The open version groups, so a version that was current or selected is still
        showing when the next session restores the selection. A group whose master is
        loaded but no longer has a version is dropped; one outside the loaded model
        cannot be judged and is kept. */
    if (dm && dm->sf) {
        QStringList expanded;
        for (const QString &m : dm->sf->expandedVersions())
            if (dm->rowFromKey(m) < 0 || !dm->versionKeysOf(m).isEmpty()) expanded << m;
        expanded.sort();
        settings->setValue("expandedVersions", expanded);
    }

    // full screen
    settings->setValue("isFullScreenFolders", fullScreenDocks.isFolders);
    settings->setValue("isFullScreenFavs", fullScreenDocks.isFavs);
    settings->setValue("isFullScreenFilters", fullScreenDocks.isFilters);
    settings->setValue("isFullScreenCatalog", fullScreenDocks.isCatalog);
    settings->setValue("isFullScreenKeywords", fullScreenDocks.isKeywords);
    settings->setValue("isFullScreenCollections", fullScreenDocks.isCollections);
    settings->setValue("isFullScreenQueries", fullScreenDocks.isQueries);
    settings->setValue("isFullScreenPlaces", fullScreenDocks.isPlaces);
    settings->setValue("isFullScreenModule", fullScreenDocks.isModule);
    settings->setValue("isFullScreenMetadata", fullScreenDocks.isMetadata);
    settings->setValue("isFullScreenDevelop", fullScreenDocks.isDevelop);
    settings->setValue("isFullScreenHistory", fullScreenDocks.isHistory);
    settings->setValue("isFullScreenEmbellish", fullScreenDocks.isEmbellish);
    settings->setValue("isFullScreenThumbs", fullScreenDocks.isThumbs);
    settings->setValue("isFullScreenStatusBar", fullScreenDocks.isStatusBar);

    // general
    settings->setValue("lastPrefPage", lastPrefPage);
    //setting->setValue("mouseClickScroll", mouseClickScroll);
    settings->setValue("toggleZoomValue", imageView->toggleZoom);
    settings->setValue("limitFit100Pct", imageView->limitFit100Pct);
    //setting->setValue("sortColumn", sortColumn);
    //setting->setValue("sortReverse", sortReverseAction->isChecked());
    settings->setValue("autoAdvance", G::autoAdvance);
    settings->setValue("turnOffEmbellish", turnOffEmbellish);
    settings->setValue("deleteWarning", deleteWarning);
    settings->setValue("pickLossWarning", pickLossWarning);
    settings->setValue("modifySourceFiles", G::modifySourceFiles);
    settings->setValue("backupBeforeModifying", G::backupBeforeModifying);
    settings->setValue("autoAddMissingThumbnails", G::autoAddMissingThumbnails);
    settings->setValue("ignoreAddThumbnailsDlg", ignoreAddThumbnailsDlg);
    settings->setValue("renderVideoThumb", G::renderVideoThumb);
    settings->setValue("isLogAllToFileForDebugging", G::isLogger);
    /* wheelSensitivityPct supersedes wheelSensitivity, which held 1 - 210 on the
       opposite (larger = less sensitive) scale.  See loadSettings for the migration. */
    settings->setValue("wheelSensitivityPct", G::wheelSensitivity);
    settings->remove("wheelSensitivity");
    settings->setValue("sliderAboveBelow", G::sliderAboveBelow);

    // datamodel
    settings->setValue("maxIconSize", G::maxIconSize);

    // appearance
    settings->setValue("textShade", G::textShade);
    settings->setValue("backgroundShade", G::backgroundShade);
    settings->setValue("fontSize", G::strFontSize);
    settings->setValue("classificationBadgeInImageDiameter", classificationBadgeInImageDiameter);
    settings->setValue("classificationBadgeSizeFactor", classificationBadgeSizeFactor);
    settings->setValue("iconNumberSize", iconNumberSize);
    settings->setValue("infoOverlayFontSize", imageView->infoOverlayFontSize);

    // files
    settings->setValue("includeSidecars", G::includeSidecars);
    settings->setValue("useRaw", G::useRaw);
    settings->setValue("decodeRawEngine", static_cast<int>(G::decodeRawEngine));
#ifdef Q_OS_MAC
    settings->setValue("appleRawDecoderVersion", AppleRawDecode::VersionPin());
#endif
    settings->setValue("rememberLastDir", rememberLastDir);
    settings->setValue("checkIfUpdate", checkIfUpdate);
    settings->setValue("restoreLibraryState", restoreLibraryState);
    settings->setValue("lastScopeWasLibrary", lastScopeWasLibrary);
    settings->setValue("updateSkipVersion", updateSkipVersion);
    settings->setValue("combineRawJpg", combineRawJpg);

    // develop
    settings->setValue("previewSource", static_cast<int>(G::previewSource));
    settings->setValue("devPreviewMaxEdge", G::devPreviewMaxEdge);
    settings->setValue("devPreviewQuality", G::devPreviewQuality);
    settings->setValue("devPreviewCacheMaxBytes", G::devPreviewCacheMaxBytes);
    settings->setValue("cacheThumbnails", G::cacheThumbnails);
    settings->setValue("maxIconChunk", G::maxIconChunk);
    settings->setValue("maxSearchResults", G::maxSearchResults);
    settings->setValue("useIndexMetadata", G::useIndexMetadata);
    settings->setValue("useScrollInVerify", G::useScrollInVerify);
    settings->setValue("autoScanCatalog", G::autoScanCatalog);
    settings->setValue("autoScanCatalogMinutes", G::autoScanCatalogMinutes);
    settings->setValue("thumbCacheMaxBytes", G::thumbCacheMaxBytes);
    settings->setValue("buildDevPreviewsInBackground", G::buildDevPreviewsInBackground);

    /* ingest (moved to MW::ingest)
    */

    // thumbs (loaded in MW::createThumbView)
    settings->setValue("thumbWidth", thumbView->iconWidth);
    settings->setValue("thumbHeight", thumbView->iconHeight);
    settings->setValue("labelFontSize", thumbView->labelFontSize);
    settings->setValue("showThumbLabels", thumbView->showIconLabels);
    settings->setValue("showZoomFrame", thumbView->showZoomFrame);

    // grid (loaded in MW::createGridView)
    /* Persist assignedIconWidth (the stable user-intended reference size), not iconWidth.
       iconWidth is the justified, viewport-dependent value: during startup the hidden
       gridView can be rejustified at a transient narrow width, shrinking iconWidth toward
       ICON_MIN (e.g. 62) while assignedIconWidth stays correct. Saving iconWidth persisted
       that transient size; assignedIconWidth survives the transient. On restore it is read
       into iconWidth and copied back to assignedIconWidth (MW::createGridView), and
       rejustify() recomputes the justified iconWidth once the grid is shown at full width. */
    settings->setValue("thumbWidthGrid", gridView->assignedIconWidth);
    settings->setValue("thumbHeightGrid", gridView->iconHeight);
    settings->setValue("labelFontSizeGrid", gridView->labelFontSize);
    settings->setValue("showThumbLabelsGrid", gridView->showIconLabels);
    settings->setValue("labelChoice", gridView->labelChoice);

    // settings->setValue("thumbsPerPage", metadataCacheThread->visibleIcons);

    // slideshow
    settings->setValue("slideShowDelay", slideShowDelay);
    settings->setValue("isSlideShowRandom", isSlideShowRandom);
    settings->setValue("isSlideShowWrap", isSlideShowWrap);
    // map (the Map module's tile provider)
    settings->setValue("mapStyle", mapStyle);
    /*  Where the map was looking, so a restart opens on the same place (MapView::
        setView). Only once the map has been shown: a session that never opened the
        Map module leaves the last real view alone. */
    if (mapView && mapView->hasBeenShown()) {
        settings->setValue("mapLat", mapView->viewLat());
        settings->setValue("mapLon", mapView->viewLon());
        settings->setValue("mapZoom", mapView->viewZoom());
    }
    settings->setValue("mapTileUrl", mapTileUrl);
    settings->setValue("mapTileKey", mapTileKey);
    settings->setValue("mapAttribution", mapAttribution);
    settings->setValue("mapMaxZoom", mapMaxZoom);

    // image cache (see preferences.cpp)
    // settings->setValue("autoMaxMB", imageCache->getAutoMaxMB());
    // settings->setValue("cacheMaxMB", imageCache->getMaxMB());

    // performance / productivity
    settings->setValue("showCacheProgress", G::showCacheProgress);
    settings->setValue("useJitIconCache", G::useJitIconCache);

    settings->setValue("isRatingBadgeVisible", ratingBadgeVisibleAction->isChecked());
    settings->setValue("isIconNumberVisible", iconNumberVisibleAction->isChecked());
    settings->setValue("isImageInfoVisible", infoVisibleAction->isChecked());
    settings->setValue("isLoupeDisplay", asLoupeAction->isChecked());
    settings->setValue("isGridDisplay", asGridAction->isChecked());
    settings->setValue("isTableDisplay", asTableAction->isChecked());
    settings->setValue("isCompareDisplay", asCompareAction->isChecked());

    settings->setValue("wasThumbDockVisible", wasThumbDockVisible);

    //settings->setValue("isMenuBarVisible", menuBarVisibleAction->isChecked());
    settings->setValue("isStatusBarVisible", statusBarVisibleAction->isChecked());
    settings->setValue("isFolderDockVisible", folderDockVisibleAction->isChecked());
    settings->setValue("isFavDockVisible", favDockVisibleAction->isChecked());
    settings->setValue("isFilterDockVisible", filterDockVisibleAction->isChecked());
    settings->setValue("isCatalogDockVisible",
                       catalogDockVisibleAction->isChecked());
    settings->setValue("isKeywordsDockVisible",
                       keywordsDockVisibleAction->isChecked());
    settings->setValue("isCollectionsDockVisible",
                       collectionsDockVisibleAction->isChecked());
    settings->setValue("isQueriesDockVisible", queriesDockVisibleAction->isChecked());
    settings->setValue("isPlacesDockVisible", placesDockVisibleAction->isChecked());

    // the show/hide bars: which sides the user left collapsed (see the function)
    writeSessionAreaCollapse();
    settings->setValue("isModuleDockVisible", moduleDockVisibleAction->isChecked());

    /* The scope table the user nominated for background cataloguing. It lives HERE and
       not in the index database: CacheDb::moveAside discards that file without asking
       when it will not open, and losing the user's choice of what to index is not the
       same kind of loss as discarding a rebuildable index.

       AN ARRAY, NOT AN ENCODED STRING LIST. A row is three fields, one of them a path,
       and a path may legally contain whatever separator character looked safe -- so a
       delimiter here would be a folder name away from corrupting the setting. The group
       is cleared first, or a table that shrank would keep the tail of the longer one. */
    /* Survives a restart, or the editor would ask for a scan that can never close the
       gap every time Winnow is launched. */
    settings->setValue("skipCatalogScopePrompt", skipCatalogScopePrompt);
    settings->setValue("skipStaleThumbPrompt", skipStaleThumbPrompt);
    settings->remove("catalogScopePrompted");    // superseded by skipCatalogScopePrompt
    settings->setValue("catalogUncatalogable", catalogUncatalogable);
    settings->setValue("catalogScanIncomplete", catalogScanIncomplete);
    settings->remove("CatalogScope");
    settings->beginWriteArray("CatalogScope");
    for (int i = 0; i < catalogScope.size(); ++i) {
        settings->setArrayIndex(i);
        settings->setValue("path", catalogScope.at(i).path);
        settings->setValue("include", catalogScope.at(i).include);
        settings->setValue("recurse", catalogScope.at(i).recurse);
    }
    settings->endArray();
    settings->setValue("isMetadataDockVisible", metadataDockVisibleAction->isChecked());
    settings->setValue("isEmbelDockVisible", embelDockVisibleAction->isChecked());
    settings->setValue("isDevelopDockVisible", developDockVisibleAction->isChecked());
    settings->setValue("isHistoryDockVisible", historyDockVisibleAction->isChecked());
    /* Presets is a SECTION of the History panel now, so this key carries whether that
       section is expanded. The section itself also persists under
       Develop/SectionExpanded/PresetsSection, which is the source of truth; this one is
       what the workspace record reads. */
    settings->setValue("isPresetsDockVisible", presetsDockVisibleAction->isChecked());
    settings->setValue("isThumbDockVisible", thumbDockVisibleAction->isChecked());

    /* Property Editor */
    settings->setValue("isSoloPrefDlg", isSoloPrefDlg);

    /* Focus Stack */
    settings->setValue("focusStackMethod", fsMethod);
    settings->setValue("focusStackRemoveTemp", fsRemoveTemp);
    settings->setValue("focusStack8bit", fs8bit);

    /* Docks are updated in DockWidget */

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

    // moved to MW::ingest
    /* Tokens used for ingest operations */
    // settings->beginGroup("PathTokens");
    // settings->remove("");
    // // save path templates
    // QMapIterator<QString, QString> pathIter(pathTemplates);
    // while (pathIter.hasNext()) {
    //     pathIter.next();
    //     settings->setValue(pathIter.key(), pathIter.value());
    // }
    // settings->endGroup();

    // save filename templates
    settings->beginGroup("FileNameTokens");
    settings->remove("");
    QMapIterator<QString, QString> filenameIter(filenameTemplates);
    while (filenameIter.hasNext()) {
        filenameIter.next();
        settings->setValue(filenameIter.key(), filenameIter.value());
    }
    settings->endGroup();

    /* Token templates used for shooting information shown in ImageView*/
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

    // dock collapse state and per-area solo mode
    settings->beginGroup("DockCollapsed");
    if (folderDock)   settings->setValue("FolderDock", folderDock->isCollapsed());
    if (favDock)      settings->setValue("BookmarkDock", favDock->isCollapsed());
    if (filterDock)   settings->setValue("FilterDock", filterDock->isCollapsed());
    if (metadataDock) settings->setValue("MetadataDock", metadataDock->isCollapsed());
    if (thumbDock)    settings->setValue("ThumbDock", thumbDock->isCollapsed());
    if (embelDock)    settings->setValue("EmbelDock", embelDock->isCollapsed());
    if (developDock)  settings->setValue("DevelopDock", developDock->isCollapsed());
    if (historyDock)  settings->setValue("HistoryDock", historyDock->isCollapsed());
    if (collectionsDock)
        settings->setValue("CollectionsDock", collectionsDock->isCollapsed());
    if (queriesDock) settings->setValue("QueriesDock", queriesDock->isCollapsed());
    if (placesDock) settings->setValue("PlacesDock", placesDock->isCollapsed());
    settings->endGroup();

    settings->beginGroup("DockSoloMode");
    settings->setValue("Left",   m_dockSoloMode.value(Qt::LeftDockWidgetArea,   false));
    settings->setValue("Right",  m_dockSoloMode.value(Qt::RightDockWidgetArea,  false));
    settings->setValue("Top",    m_dockSoloMode.value(Qt::TopDockWidgetArea,    false));
    settings->setValue("Bottom", m_dockSoloMode.value(Qt::BottomDockWidgetArea, false));
    settings->endGroup();
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

    /* FACTORY SETTINGS
       What a new user starts with: these are assigned only when settings.ini has no
       settings yet (!isSettings).  Everything after this section reads the saved values,
       and a preference that is not set here keeps its C++ initializer for a new user, so
       every preference belongs here unless its owner does not exist yet (the thumbnail /
       grid view defaults are in MW::createThumbView / createGridView, the image cache
       size in MW::createImageCache, limitFit100Pct in MW::createImageView).  Give a new
       key the same fallback where it is read below (settings->value(key, default)), or
       an existing user without the key gets a different value from a new user.

       factorySetting(var, "key", code value, isNew): "key" is the QSettings key the
       value is saved under.  A factory test (Preferences > Test factory settings, Rory
       only) can retune any keyed value: "Restore my settings" offers each one changed
       during the test, and the accepted values go to Settings/factory.ini, which
       overrides the code value here.  Values with no key (session state, debug flags)
       are plain assignments and cannot be tuned that way.
    */
    const bool isNew = !isSettings || simulateJustInstalled;
    loadFactoryOverrides();
    factoryValues.clear();
    factoryCodeValues.clear();

    // general
    factorySetting(combineRawJpg, "combineRawJpg", false, isNew);
    factorySetting(G::autoAdvance, "autoAdvance", false, isNew);
    factorySetting(turnOffEmbellish, "turnOffEmbellish", true, isNew);
    factorySetting(G::renderVideoThumb, "renderVideoThumb", true, isNew);
    factorySetting(G::wheelSensitivity, "wheelSensitivityPct", 50, isNew);  // percent
    factorySetting(G::modifySourceFiles, "modifySourceFiles", false, isNew);
    factorySetting(G::backupBeforeModifying, "backupBeforeModifying", false, isNew);
    factorySetting(G::autoAddMissingThumbnails, "autoAddMissingThumbnails", false, isNew);
    factorySetting(ignoreAddThumbnailsDlg, "ignoreAddThumbnailsDlg", false, isNew);
    factorySetting(deleteWarning, "deleteWarning", true, isNew);
    factorySetting(pickLossWarning, "pickLossWarning", true, isNew);
    factorySetting(rememberLastDir, "rememberLastDir", false, isNew);
    factorySetting(checkIfUpdate, "checkIfUpdate", true, isNew);
    factorySetting(restoreLibraryState, "restoreLibraryState", false, isNew);

    // appearance
    factorySetting(G::textShade, "textShade", 190, isNew);
    factorySetting(G::backgroundShade, "backgroundShade", 50, isNew);
    factorySetting(G::strFontSize, "fontSize", "12", isNew);
    factorySetting(infoOverlayFontSize, "infoOverlayFontSize", 24, isNew);
    factorySetting(classificationBadgeInImageDiameter, "classificationBadgeInImageDiameter",
                   32, isNew);
    factorySetting(classificationBadgeSizeFactor, "classificationBadgeSizeFactor", 14, isNew);
    factorySetting(iconNumberSize, "iconNumberSize", 24, isNew);
    factorySetting(isRatingBadgeVisible, "isRatingBadgeVisible", true, isNew);
    factorySetting(isIconNumberVisible, "isIconNumberVisible", true, isNew);

    // datamodel
    factorySetting(G::maxIconSize, "maxIconSize", 256, isNew);

    // files
    factorySetting(G::useRaw, "useRaw", false, isNew);
    /* Default raw renderer (Preferences > Develop): the engine, and on macOS the Apple
       decoder version pin (0 = newest). */
    int rawEngine = static_cast<int>(G::decodeRawEngine);
    factorySetting(rawEngine, "decodeRawEngine",
                   static_cast<int>(G::DecodeRawEngine::winnowDecodeRawEngine), isNew);
#ifdef Q_OS_MAC
    int applePin = AppleRawDecode::VersionPin();
    factorySetting(applePin, "appleRawDecoderVersion", 0, isNew);
#endif

    // develop previews, thumbnails, catalog
    int previewSource = static_cast<int>(G::previewSource);
    factorySetting(previewSource, "previewSource",
                   static_cast<int>(G::PreviewSource::Developed), isNew);
    factorySetting(G::devPreviewMaxEdge, "devPreviewMaxEdge", G::kDevPreviewSizeFull, isNew);
    factorySetting(G::devPreviewQuality, "devPreviewQuality", G::kDevPreviewQualityDefault,
                   isNew);
    factorySetting(G::devPreviewCacheMaxBytes, "devPreviewCacheMaxBytes",
                   20LL * 1024 * 1024 * 1024, isNew);
    factorySetting(G::cacheThumbnails, "cacheThumbnails", true, isNew);
    factorySetting(G::maxIconChunk, "maxIconChunk", 10000, isNew);
    factorySetting(G::maxSearchResults, "maxSearchResults", 0, isNew);  // 0 = no limit
    factorySetting(G::useIndexMetadata, "useIndexMetadata", false, isNew);
    factorySetting(G::useScrollInVerify, "useScrollInVerify", true, isNew);
    factorySetting(G::autoScanCatalog, "autoScanCatalog", true, isNew);
    factorySetting(G::autoScanCatalogMinutes, "autoScanCatalogMinutes", 60, isNew);
    factorySetting(G::thumbCacheMaxBytes, "thumbCacheMaxBytes",
                   5LL * 1024 * 1024 * 1024, isNew);
    factorySetting(G::buildDevPreviewsInBackground, "buildDevPreviewsInBackground", false,
                   isNew);

    // ingest
    factorySetting(autoIngestFolderPath, "autoIngestFolderPath", false, isNew);
    factorySetting(autoEjectUsb, "autoEjectUSB", false, isNew);
    factorySetting(integrityCheck, "integrityCheck", false, isNew);
    factorySetting(isBackgroundIngest, "isBackgroundIngest", false, isNew);
    factorySetting(isBackgroundIngestBeep, "isBackgroundIngestBeep", false, isNew);
    factorySetting(ingestIncludeXmpSidecar, "ingestIncludeXmpSidecar", true, isNew);
    factorySetting(gotoIngestFolder, "gotoIngestFolder", false, isNew);
    factorySetting(backupIngest, "backupIngest", false, isNew);
    factorySetting(pathTemplateSelected, "pathTemplateSelected", 0, isNew);
    factorySetting(pathTemplateSelected2, "pathTemplateSelected2", 0, isNew);
    factorySetting(filenameTemplateSelected, "filenameTemplateSelected", 0, isNew);

    // preferences
    factorySetting(isSoloPrefDlg, "isSoloPrefDlg", true, isNew);

    // slideshow
    factorySetting(slideShowDelay, "slideShowDelay", 5, isNew);
    factorySetting(isSlideShowRandom, "isSlideShowRandom", false, isNew);
    factorySetting(isSlideShowWrap, "isSlideShowWrap", true, isNew);

    // performance / productivity
    factorySetting(G::showCacheProgress, "showCacheProgress", false, isNew);
    factorySetting(G::useJitIconCache, "useJitIconCache", false, isNew);

    // focus stack
    factorySetting(fsMethod, "focusStackMethod", FS::MethodsString.at(FS::Methods::DMap),
                   isNew);
    factorySetting(fsRemoveTemp, "focusStackRemoveTemp", true, isNew);
    factorySetting(fs8bit, "focusStack8bit", true, isNew);

    if (isNew) {
        // derived from the factory values above
        G::combineRawJpg = combineRawJpg;
        G::backgroundColor = QColor(G::backgroundShade, G::backgroundShade, G::backgroundShade);
        G::fontSize = G::strFontSize.toInt();
        G::previewSource = static_cast<G::PreviewSource>(previewSource);
        G::decodeRawEngine = rawEngine == int(G::DecodeRawEngine::winnowDecodeRawEngine)
                           ? G::DecodeRawEngine::winnowDecodeRawEngine
                           : G::DecodeRawEngine::appleDecodeRawEngine;
#ifdef Q_OS_MAC
        AppleRawDecode::SetVersionPin(applePin);
#else
        // the Apple decoder is macOS-only (see the decodeRawEngine read below)
        G::decodeRawEngine = G::DecodeRawEngine::winnowDecodeRawEngine;
#endif

        // not settings a user tunes: session state and debug flags
        prevMode = "Loupe";
        G::mode = "Loupe";
        isLogAllToFileForDebugging = false;
        G::isLogger = false;
        G::isRunByExtern = false;
        G::isIssueLogger = false;
        sortColumn = G::NameColumn;
        lastScopeWasLibrary = false;
        updateSkipVersion = "";
        lastDir = "";

        return true;
    }
    // end factory settings

    // Get settings saved from last session

    // general
    G::autoAdvance = settings->value("autoAdvance").toBool();
    turnOffEmbellish = settings->value("turnOffEmbellish", true).toBool();
    isLogAllToFileForDebugging = settings->value("isLogAllToFileForDebugging").toBool();

    if (settings->contains("deleteWarning"))
        deleteWarning = settings->value("deleteWarning").toBool();
    else
        deleteWarning = true;

    if (settings->contains("pickLossWarning"))
        pickLossWarning = settings->value("pickLossWarning").toBool();
    else
        pickLossWarning = true;

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

    if (settings->contains("renderVideoThumb"))
        G::renderVideoThumb = settings->value("renderVideoThumb").toBool();
    else
        G::renderVideoThumb = true;

    /* Mouse wheel sensitivity is now a percent (1 - 100, larger = more sensitive).
       The superseded wheelSensitivity key held 1 - 210 on the opposite scale, so an
       existing setting is converted rather than read as a percent, which would
       reverse the user's choice and clip everything above 100. */
    if (settings->contains("wheelSensitivityPct")) {
        G::wheelSensitivity = qBound(1, settings->value("wheelSensitivityPct").toInt(), 100);
    }
    else if (settings->contains("wheelSensitivity")) {
        int prev = qBound(1, settings->value("wheelSensitivity").toInt(), 210);
        G::wheelSensitivity = qRound(100.0 - (prev - 1) * 99.0 / 209.0);
    }
    else {
        G::wheelSensitivity = 50;
    }

    G::sliderAboveBelow = settings->value("sliderAboveBelow", false).toBool();
    // Develop's own slider layout override: absent = follow the preference above
    PropertyEditor::developSliderLayout = settings->contains("Develop/sliderAboveBelow")
        ? (settings->value("Develop/sliderAboveBelow").toBool() ? 1 : 0) : -1;

    if (settings->contains("pickAudioVolume")) {
        pickClickvolume = settings->value("pickAudioVolume").toFloat() / 100;
        pickClick->setVolume(pickClickvolume);
    }

    lastFileIfCrash = settings->value("lastFileSelection").toString();

    // appearance
    if (settings->contains("textShade")) {
        G::textShade = settings->value("textShade").toInt();
        if (G::textShade < 155 || G::textShade > 255) G::textShade = 190;
    }
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

//    if (settings->contains("isRatingBadgeVisible")) isRatingBadgeVisible = settings->value("isRatingBadgeVisible").toBool();
//    if (settings->contains("isIconNumberVisible")) isIconNumberVisible = settings->value("isIconNumberVisible").toBool();
    isRatingBadgeVisible = true;
    isIconNumberVisible = true;

    // datamodel
    if (settings->contains("maxIconSize")) G::maxIconSize = settings->value("maxIconSize").toInt();

    // performance / productivity
    if (settings->contains("showCacheProgress")) G::showCacheProgress = settings->value("showCacheProgress").toBool();
    if (settings->contains("useJitIconCache")) G::useJitIconCache = settings->value("useJitIconCache").toBool();

    // files
    if (settings->contains("includeSidecars")) G::includeSidecars = settings->value("includeSidecars").toBool();
    /* "colorManage" in older settings files is ignored: colour management is always on
       (2026-10-26, notes/Documentation.md 15.2.2). */
    if (settings->contains("useRaw")) G::useRaw = settings->value("useRaw").toBool();
    if (settings->contains("decodeRawEngine")) {
        /* Sticky RAW decode engine (Develop "Demosaic" combo). appleDecodeRawEngine is
           macOS-only; off-mac the decode callers fall back to winnow anyway, but
           normalize here so the combo and applyCoreVisibility reflect the engine that
           will run.

           VALIDATE BEFORE CASTING. The stored value is a plain int and the cast is
           unchecked, so a damaged or newer-build setting could name an engine this
           build has no enumerator for. That does not merely pick the wrong decoder --
           it matches NEITHER branch downstream, so RawFormat skips the Apple path AND
           the winnow-only PMRID raw denoise, leaving the user with silently missing
           noise reduction. Fall back to the portable engine and say so. */
        bool ok = false;
        const int raw = settings->value("decodeRawEngine").toInt(&ok);
        auto e = G::DecodeRawEngine::winnowDecodeRawEngine;
        if (!ok || raw < int(G::DecodeRawEngine::winnowDecodeRawEngine) ||
                   raw > int(G::DecodeRawEngine::appleDecodeRawEngine)) {
            /* Deferred: G::popup does not exist this early in the constructor. */
            G::startupWarnings << "The saved raw decode engine was not recognized.<br>"
                                  "Using the Winnow decoder -- check Develop > Demosaic.";
        }
        else {
            e = static_cast<G::DecodeRawEngine>(raw);
        }
#ifndef Q_OS_MAC
        if (e == G::DecodeRawEngine::appleDecodeRawEngine)
            e = G::DecodeRawEngine::winnowDecodeRawEngine;
#endif
        G::decodeRawEngine = e;
    }
    else if (rawEngine == int(G::DecodeRawEngine::winnowDecodeRawEngine)) {
        // no saved engine: the factory one (Factory settings above)
        G::decodeRawEngine = G::DecodeRawEngine::winnowDecodeRawEngine;
    }
#ifdef Q_OS_MAC
    /* The Apple decoder version pin (Raw panel "Apple Raw N"); 0 = newest. A pin newer
       than this macOS offers is harmless: each file takes its newest version <= it. */
    if (settings->contains("appleRawDecoderVersion"))
        AppleRawDecode::SetVersionPin(settings->value("appleRawDecoderVersion").toInt());
#endif
    // develop
    /* Develop previews. previewSource is VALIDATED BEFORE CASTING for the same reason as
       the two enums above -- a damaged int would name a branch this build has no code
       for. The sizes and the quality are plain numbers, but an absurd one would either
       disable the cache, let it eat the disk, or write previews nothing can use, so each
       is range-checked and a value outside its band is ignored rather than adopted. */
    if (settings->contains("previewSource")) {
        bool ok = false;
        const int src = settings->value("previewSource").toInt(&ok);
        if (ok && src >= int(G::PreviewSource::Original)
               && src <= int(G::PreviewSource::Developed))
            G::previewSource = static_cast<G::PreviewSource>(src);
    }
    if (settings->contains("devPreviewMaxEdge")) {
        const int edge = settings->value("devPreviewMaxEdge").toInt();
        /* 0 = Full (no cap). Anything else must be big enough to be worth caching. */
        if (edge == G::kDevPreviewSizeFull || edge >= 1024) G::devPreviewMaxEdge = edge;
    }
    if (settings->contains("devPreviewQuality")) {
        const int q = settings->value("devPreviewQuality").toInt();
        /* A JPEG quality outside the band the spinbox offers is a damaged setting, so it
           is ignored rather than clamped -- adopting it would write every future preview
           at a quality no control can show or undo. */
        if (q >= G::kDevPreviewQualityMin && q <= G::kDevPreviewQualityMax)
            G::devPreviewQuality = q;
    }
    if (settings->contains("devPreviewCacheMaxBytes")) {
        const qint64 cap = settings->value("devPreviewCacheMaxBytes").toLongLong();
        if (cap >= 256LL * 1024 * 1024) G::devPreviewCacheMaxBytes = cap;
    }
    if (settings->contains("cacheThumbnails"))
        G::cacheThumbnails = settings->value("cacheThumbnails").toBool();
    if (settings->contains("maxIconChunk")) {
        const int n = settings->value("maxIconChunk").toInt();
        /* 0 is "no limit" and is legal; a negative is a damaged setting. */
        if (n >= 0) G::maxIconChunk = n;
    }
    if (settings->contains("maxSearchResults")) {
        const int n = settings->value("maxSearchResults").toInt();
        /*  THE OLD 5,000 DEFAULT IS RETIRED ONCE, not honoured. It was saved for every
            existing user, so leaving it in place would keep their Catalog scope capped
            at a number that only ever existed because a row cost 20 KB and had to be
            read from its file -- and the point of the change is that picking the catalog
            now shows the catalog. A user who deliberately chose 5,000 loses that choice
            here, which is the cost of not being able to tell the two apart; every other
            value is kept. */
        if (n == 5000) settings->remove("maxSearchResults");
        else if (n >= 0) G::maxSearchResults = n;
    }
    if (settings->contains("useIndexMetadata"))
        G::useIndexMetadata = settings->value("useIndexMetadata").toBool();
    if (settings->contains("useScrollInVerify"))
        G::useScrollInVerify = settings->value("useScrollInVerify").toBool();
    if (settings->contains("autoScanCatalog"))
        G::autoScanCatalog = settings->value("autoScanCatalog").toBool();
    if (settings->contains("autoScanCatalogMinutes")) {
        /*  Clamped on the way in, not just in the spinbox: a settings file edited by hand
            (or written by an older build) could hold 0, which would make every selection
            of the Library start a scope walk. */
        const int m = settings->value("autoScanCatalogMinutes").toInt();
        if (m > 0) G::autoScanCatalogMinutes = qBound(1, m, 1440);
    }

    /*  A/B KNOBS FOR THE HEADLESS RUNS, and only for those. The load-pipeline work is
        verified by running the same folder twice with one thing changed, so the two
        things most often changed have to be reachable without editing a default and
        rebuilding:

          WINNOW_PERF_PROBE=1      emit the [PERF] Phase 1/2 timing lines
          WINNOW_INDEX_METADATA=1  fill a row from the local index when it can answer
                                   (0 forces it off, overriding the saved preference)
          WINNOW_MAX_ICON_CHUNK=<n> ceiling on how many icons a load may hold, which is
                                   what decides how much of a load is icon work

        Applied AFTER the preference is read, so the environment wins, and only in test
        mode (QStandardPaths::setTestModeEnabled, which main sets for --selftest and
        friends): in an ordinary session these are preferences, and an environment
        variable that silently changed how metadata is read would be a trap. */
    if (QStandardPaths::isTestModeEnabled()) {
        if (qEnvironmentVariableIntValue("WINNOW_PERF_PROBE") == 1) G::isPerfProbe = true;
        if (!qEnvironmentVariable("WINNOW_INDEX_METADATA").isNull())
            G::useIndexMetadata = qEnvironmentVariableIntValue("WINNOW_INDEX_METADATA") == 1;
        if (!qEnvironmentVariable("WINNOW_MAX_ICON_CHUNK").isNull()) {
            const int n = qEnvironmentVariableIntValue("WINNOW_MAX_ICON_CHUNK");
            if (n >= 0) G::maxIconChunk = n;
        }
    }
    if (settings->contains("thumbCacheMaxBytes")) {
        const qint64 cap = settings->value("thumbCacheMaxBytes").toLongLong();
        if (cap >= 256LL * 1024 * 1024) G::thumbCacheMaxBytes = cap;
    }
    if (settings->contains("buildDevPreviewsInBackground"))
        G::buildDevPreviewsInBackground =
            settings->value("buildDevPreviewsInBackground").toBool();
    /* The cache reads its index lazily, on first use, so applying the cap here (before any
       folder loads) is early enough and does not force a load. */
    DevPreviewCache::instance().setMaxBytes(G::devPreviewCacheMaxBytes);
    ThumbCache::instance().setMaxBytes(G::thumbCacheMaxBytes);

    // if (settings->contains("rememberLastDir")) rememberLastDir = settings->value("rememberLastDir").toBool();
    rememberLastDir = false;    // remove rememberLastDir for now 2025-03-21
    if (settings->contains("checkIfUpdate")) checkIfUpdate = settings->value("checkIfUpdate").toBool();
    if (settings->contains("restoreLibraryState"))
        restoreLibraryState = settings->value("restoreLibraryState").toBool();
    /*  MISSING = the retired "Open library at start": a user who had it on always came
        up in the Library, so the first start after the upgrade still does. From the next
        close on the key records the truth, and the old one is dropped. */
    lastScopeWasLibrary = settings->value("lastScopeWasLibrary",
                                          settings->value("openLibraryAtStart", false))
                              .toBool();
    settings->remove("openLibraryAtStart");
    if (settings->contains("updateSkipVersion")) updateSkipVersion = settings->value("updateSkipVersion").toString();
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

    // map
    mapStyle = settings->value("mapStyle", "standard").toString();
    mapTileUrl = settings->value("mapTileUrl").toString();
    mapTileKey = settings->value("mapTileKey").toString();
    mapAttribution = settings->value("mapAttribution").toString();
    mapMaxZoom = settings->value("mapMaxZoom", 19).toInt();

    // image cache (see initialize.cpp MW::createImageCache)

    // full screen
    if (settings->contains("isFullScreenFolders")) fullScreenDocks.isFolders = settings->value("isFullScreenFolders").toBool();
    if (settings->contains("isFullScreenFavs")) fullScreenDocks.isFavs = settings->value("isFullScreenFavs").toBool();
    if (settings->contains("isFullScreenFilters")) fullScreenDocks.isFilters = settings->value("isFullScreenFilters").toBool();
    if (settings->contains("isFullScreenCatalog"))
        fullScreenDocks.isCatalog = settings->value("isFullScreenCatalog").toBool();
    if (settings->contains("isFullScreenKeywords"))
        fullScreenDocks.isKeywords = settings->value("isFullScreenKeywords").toBool();
    if (settings->contains("isFullScreenCollections"))
        fullScreenDocks.isCollections =
            settings->value("isFullScreenCollections").toBool();
    if (settings->contains("isFullScreenQueries"))
        fullScreenDocks.isQueries = settings->value("isFullScreenQueries").toBool();
    if (settings->contains("isFullScreenPlaces"))
        fullScreenDocks.isPlaces = settings->value("isFullScreenPlaces").toBool();
    if (settings->contains("isFullScreenModule"))
        fullScreenDocks.isModule = settings->value("isFullScreenModule").toBool();
    if (settings->contains("isFullScreenMetadata")) fullScreenDocks.isMetadata = settings->value("isFullScreenMetadata").toBool();
    if (settings->contains("isFullScreenDevelop")) fullScreenDocks.isDevelop = settings->value("isFullScreenDevelop").toBool();
    if (settings->contains("isFullScreenHistory")) fullScreenDocks.isHistory = settings->value("isFullScreenHistory").toBool();
    if (settings->contains("isFullScreenEmbellish")) fullScreenDocks.isEmbellish = settings->value("isFullScreenEmbellish").toBool();
    if (settings->contains("isFullScreenThumbs")) fullScreenDocks.isThumbs = settings->value("isFullScreenThumbs").toBool();
    if (settings->contains("isFullScreenStatusBar")) fullScreenDocks.isStatusBar = settings->value("isFullScreenStatusBar").toBool();

    // Focus Stack
    if (settings->contains("focusStackMethod")) fsMethod = settings->value("focusStackMethod").toString();
    if (settings->contains("focusStackRemoveTemp")) fsRemoveTemp = settings->value("focusStackRemoveTemp").toBool();
    fs8bit = settings->value("focusStack8bit", true).toBool();

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

    /* read the catalog scope (see the write side for why it is not in the db) */
    skipCatalogScopePrompt = settings->value("skipCatalogScopePrompt", false).toBool();
    skipStaleThumbPrompt = settings->value("skipStaleThumbPrompt", false).toBool();
    catalogUncatalogable = settings->value("catalogUncatalogable", 0).toInt();
    catalogScanIncomplete = settings->value("catalogScanIncomplete", false).toBool();
    catalogScope.clear();
    int scopeRows = settings->beginReadArray("CatalogScope");
    for (int i = 0; i < scopeRows; ++i) {
        settings->setArrayIndex(i);
        CatalogScopeEntry e;
        e.path = catalogScopeNormalize(settings->value("path").toString());
        e.include = settings->value("include", true).toBool();
        e.recurse = settings->value("recurse", true).toBool();
        if (!e.path.isEmpty()) catalogScope << e;
    }
    settings->endArray();

    /* MIGRATION from the two-list settings this replaced (catalogRoots + one global
       catalogRootsRecurse, plus catalogExcludes). Read only when the table is empty, so
       an upgraded user keeps what they nominated instead of finding the catalog scope
       silently emptied. The old keys are left in place: a downgrade should still find
       them, and they cost nothing. */
    if (scopeRows == 0 && settings->contains("catalogRoots")) {
        const bool recurse = settings->value("catalogRootsRecurse", true).toBool();
        const QStringList roots = settings->value("catalogRoots").toStringList();
        for (const QString &path : roots) {
            CatalogScopeEntry e;
            /* The old keys were written from the folder tree, which spells a folder with
               a trailing slash; every rule in the scope table is a prefix test, so a
               migrated path has to be normalised or it matches nothing. */
            e.path = catalogScopeNormalize(path);
            e.recurse = recurse;
            if (!e.path.isEmpty()) catalogScope << e;
        }
        const QStringList excludes = settings->value("catalogExcludes").toStringList();
        for (const QString &path : excludes) {
            CatalogScopeEntry e;
            e.path = catalogScopeNormalize(path);
            e.include = false;
            if (!e.path.isEmpty()) catalogScope << e;
        }
    }

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

    // dock solo-mode state per area (collapsed-state itself is applied
    // post-restoreState in showEvent — see MW::applyDockCollapseState)
    settings->beginGroup("DockSoloMode");
    m_dockSoloMode.insert(Qt::LeftDockWidgetArea,   settings->value("Left",   false).toBool());
    m_dockSoloMode.insert(Qt::RightDockWidgetArea,  settings->value("Right",  false).toBool());
    m_dockSoloMode.insert(Qt::TopDockWidgetArea,    settings->value("Top",    false).toBool());
    m_dockSoloMode.insert(Qt::BottomDockWidgetArea, settings->value("Bottom", false).toBool());
    settings->endGroup();

    migrateSortColumnLayout();
    loadWorkspaces();
    // moved read workspaces to separate function as req'd by actions while the
    // rest of QSettings are dependent on actions being defined first.

    return true;
}

void MW::migrateOldSettings()
{
/*
    Sandboxed applications (req'd for Mac App Store) cannot access library/preferences
    so the settings need to be moved from com.Winnow.winnow_100.plist to a sandbox-safe
    .ini file
*/
    // Define new INI path inside standard writable location
    QString iniPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(iniPath);  // Ensure the folder exists
    QString iniFile = iniPath + "/settings.ini";

    // Check if the INI file already exists (migration done)
    if (QFile::exists(iniFile)) {
        // qDebug() << "✅ Settings already migrated to" << iniFile;
        return;
    }

    // qDebug() << "🔄 Migrating old settings to" << iniFile;

    // Load old settings (macOS plist format, typically in ~/Library/Preferences)
    QSettings oldSettings("Winnow", "winnow_100");

    // Load new INI-format settings
    QSettings newSettings(iniFile, QSettings::IniFormat);

    // Migrate all keys from old to new
    QStringList keys = oldSettings.allKeys();
    for (const QString &key : keys) {
        QVariant value = oldSettings.value(key);
        newSettings.setValue(key, value);
        qDebug() << "Migrated:" << key << "=>" << value;
    }

    newSettings.sync();
    qDebug() << "✅ Migration complete. INI file created at:" << iniFile;

}

/* Files beside settings.ini used by the factory reset (MW::requestSettingsReset). */
static const char *kResetRequest = "settings-reset-request.txt";  // the step to apply
static const char *kBeforeReset  = "settings-before-factory-reset.ini";
static const char *kTestBackup   = "settings-factory-test-backup.ini";

static QString settingsFile(const char *name)
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + name;
}

void MW::requestSettingsReset(const QString &step)
{
/*
    The Factory reset / Test factory settings buttons at the bottom of Preferences
    (PreferencesDlg).  The new-user settings are what MW::loadSettings applies
    when settings.ini has none (!isSettings), and every object built after it reads
    QSettings too, so the only faithful way to show them is a launch with no settings.ini.
    This cannot simply delete the file: closeEvent writes the whole session back
    (writeSettings, then saveLibraryState, hasCrashed, sync).  So the step is written to
    kResetRequest, Winnow quits normally, and the next launch carries it out in
    applyPendingSettingsReset before settings.ini is opened.  A helper started now
    (AppInstaller::relaunchAfterExit) opens Winnow again once this process has exited.

    Steps:
        "reset"    factory settings; the old settings.ini is kept as kBeforeReset
        "test"     (G::isRory) factory settings; the old settings.ini waits in kTestBackup
        "restore"  end the test: kTestBackup goes back to settings.ini

    Only settings.ini changes.  userdata.db (keywords, presets, places ...), the index,
    the thumbnail and preview caches and downloaded models are kept.
*/
    if (G::isLogger) G::log("MW::requestSettingsReset", step);

    QString title, text, info, okText;
    if (step == "reset") {
        title = "Factory Reset";
        text = "Reset Winnow to the settings a new installation starts with?";
        info = "Every preference, workspace, panel layout, shortcut and remembered "
               "choice returns to its default, and Winnow restarts.\n\n"
               "Your keywords, develop presets, places, catalog and caches are not "
               "affected.  The current settings are kept in\n"
               + QDir::toNativeSeparators(settingsFile(kBeforeReset));
        okText = "Reset and Restart";
    }
    else if (step == "test") {
        title = "Test Factory Settings";
        text = "Restart Winnow with the settings a new user sees?";
        info = "Your current settings are set aside.  Change anything you like while "
               "testing: \"Restore my settings\" at the bottom of Preferences brings back "
               "your own settings exactly as they are now.  It also offers each factory "
               "setting you changed as a new factory setting (Settings/factory.ini); "
               "nothing else done while testing is kept.";
        okText = "Restart with Factory Settings";
    }
    else if (step == "restore") {
        title = "Restore My Settings";
        text = "End the factory settings test and restart with your own settings?";
        info = "Changes made while testing factory settings are discarded.";
        okText = "Restore and Restart";
    }
    else return;

    /* The Preferences dialog stays on top, so a box parented to MW could open under it. */
    QWidget *parent = preferencesDlg ? static_cast<QWidget*>(preferencesDlg) : this;

    /* Ending a test offers what was changed as new factory settings, when there is any. */
    bool asked = false;
    if (step == "restore") {
        int answer = confirmRestoreAndFactory(parent);
        if (answer == 0) return;
        asked = answer == 1;
    }
    if (!asked) {
        QMessageBox box(parent);
        box.setWindowTitle(title);
        box.setIcon(QMessageBox::Question);
        box.setText(text);
        box.setInformativeText(info);
        QPushButton *okBtn = box.addButton(okText, QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != okBtn) return;
    }

    QFile request(settingsFile(kResetRequest));
    if (!request.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(parent, title, "Could not write\n"
                             + QDir::toNativeSeparators(request.fileName()));
        return;
    }
    request.write(step.toUtf8());
    request.close();

    QString error;
    if (!AppInstaller::relaunchAfterExit(error)) {
        QMessageBox::information(parent, title, error + "\n\nWinnow will close.  "
                                 "Open it again to finish.");
    }
    if (!close()) {
        G::popup->showPopup("Winnow will restart with the new settings when it closes.",
                            4000);
    }
}

QString MW::factoryOverridesPath(bool *inSourceTree) const
{
/*
    Settings/factory.ini: the factory values tuned by factory tests (see the Factory
    settings section of MW::loadSettings).  In a build with its source tree
    (WINNOW_SOURCE_DIR, as for Workspaces/defaults.json) it is the file in the tree, so a
    tuned value is used at once and ships with the next build.  Otherwise it is the copy
    compiled into the resource.
*/
    #ifdef WINNOW_SOURCE_DIR
    if (QFileInfo::exists(QString(WINNOW_SOURCE_DIR) + "/winnow.qrc")) {
        if (inSourceTree) *inSourceTree = true;
        return QString(WINNOW_SOURCE_DIR) + "/Settings/factory.ini";
    }
    #endif
    if (inSourceTree) *inSourceTree = false;
    return ":/Settings/factory.ini";
}

void MW::loadFactoryOverrides()
{
    factoryOverrides.clear();
    const QString path = factoryOverridesPath();
    if (!QFile::exists(path)) return;
    QSettings f(path, QSettings::IniFormat);
    for (const QString &key : f.allKeys()) factoryOverrides.insert(key, f.value(key));
}

bool MW::saveFactoryOverrides(const QStringList &keys, QString &path, QString &err)
{
/*
    Writes the current value of each key into Settings/factory.ini as its factory value.
    A value back at the code value is removed instead, so the file only ever holds what
    differs from MW::loadSettings.  Without a source tree the file goes to the desktop
    and the caller says where.
*/
    bool inSourceTree = false;
    path = factoryOverridesPath(&inSourceTree);
    if (!inSourceTree) {
        path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
             + "/factory.ini";
    }
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSettings f(path, QSettings::IniFormat);
    for (const QString &key : keys) {
        const QVariant v = settings->value(key);
        if (v.toString() == factoryCodeValues.value(key).toString()) f.remove(key);
        else f.setValue(key, v);
    }
    f.sync();
    if (f.status() != QSettings::NoError) {
        err = "Could not write " + QDir::toNativeSeparators(path);
        return false;
    }
    return true;
}

int MW::confirmRestoreAndFactory(QWidget *parent)
{
/*
    "Restore my settings" while a factory test runs.  The session is saved first
    (writeSettings), then every factory setting (factoryValues, see MW::loadSettings)
    whose saved value differs from its factory value is listed, checked, as a candidate
    new factory value.  The checked ones are written to Settings/factory.ini
    (saveFactoryOverrides); everything else from the test is discarded with it.

    Returns 0 cancelled, 1 confirmed here, -1 nothing changed (the caller asks the plain
    restore question instead).
*/
    writeSettings();
    settings->sync();

    QStringList changed;
    for (auto it = factoryValues.cbegin(); it != factoryValues.cend(); ++it) {
        if (!settings->contains(it.key())) continue;
        if (settings->value(it.key()).toString() != it.value().toString())
            changed << it.key();
    }
    if (changed.isEmpty()) return -1;

    bool inSourceTree = false;
    QString path = factoryOverridesPath(&inSourceTree);
    if (!inSourceTree) path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
                            + "/factory.ini";

    QDialog dlg(parent);
    dlg.setWindowTitle("Restore My Settings");
    dlg.setStyleSheet(G::css);
    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QLabel *intro = new QLabel("These factory settings were changed during the test.  The "
                               "checked ones become the new factory settings: what a new "
                               "user starts with.  Then Winnow restarts with your own "
                               "settings.", &dlg);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QTreeWidget *list = new QTreeWidget(&dlg);
    list->setRootIsDecorated(false);
    list->setHeaderLabels({"Setting", "Factory", "Changed to"});
    for (const QString &key : changed) {
        auto *item = new QTreeWidgetItem(list, {key, factoryValues.value(key).toString(),
                                                settings->value(key).toString()});
        item->setCheckState(0, Qt::Checked);
    }
    for (int c = 0; c < 3; ++c) list->resizeColumnToContents(c);
    layout->addWidget(list);

    QLabel *where = new QLabel("Saved to " + QDir::toNativeSeparators(path)
                               + (inSourceTree ? "" : "\n(no source tree: copy it to "
                                                      "Settings/factory.ini and rebuild)"),
                               &dlg);
    where->setWordWrap(true);
    where->setStyleSheet("color: " + G::secondaryTextColor.name() + ";");
    layout->addWidget(where);

    QDialogButtonBox *btns = new QDialogButtonBox(&dlg);
    btns->addButton("Restore and Save Factory Settings", QDialogButtonBox::AcceptRole);
    btns->addButton(QDialogButtonBox::Cancel);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(btns);
    dlg.resize(560, 360);
    if (dlg.exec() != QDialog::Accepted) return 0;

    QStringList keys;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = list->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) keys << item->text(0);
    }
    if (keys.isEmpty()) return 1;

    QString err;
    if (!saveFactoryOverrides(keys, path, err)) {
        QMessageBox::warning(parent, "Restore My Settings",
                             err + "\n\nNothing was restored.");
        return 0;
    }
    return 1;
}

void MW::applyPendingSettingsReset()
{
/*
    Carries out the step MW::requestSettingsReset left in kResetRequest, then sets
    isFactorySettingsTest.  Called from the MW constructor BEFORE migrateOldSettings and
    before settings.ini is opened.  After "reset" or "test" an EMPTY settings.ini is left
    in place: with no file at all migrateOldSettings would copy the old pre-ini plist
    settings back in.  An empty file has no "slideShowDelay", so isSettings is false and
    loadSettings applies the factory defaults.
*/
    if (G::isLogger) G::log("MW::applyPendingSettingsReset");
    const QString ini = settingsFile("settings.ini");
    const QString backup = settingsFile(kTestBackup);

    QFile request(settingsFile(kResetRequest));
    if (request.open(QIODevice::ReadOnly)) {
        const QString step = QString::fromUtf8(request.readAll()).trimmed();
        request.close();
        request.remove();

        auto startEmpty = [&ini]() {
            QFile f(ini);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.close();
        };

        if (step == "reset") {
            /* During a test settings.ini is only the test's: the user's own settings
               stay in the test backup, so keep that and do not save the test's. */
            if (!QFile::exists(backup)) {
                QFile::remove(settingsFile(kBeforeReset));
                QFile::rename(ini, settingsFile(kBeforeReset));
            }
            QFile::remove(ini);
            startEmpty();
        }
        else if (step == "test") {
            if (!QFile::exists(backup)) QFile::rename(ini, backup);
            QFile::remove(ini);
            startEmpty();
        }
        else if (step == "restore") {
            if (QFile::exists(backup)) {
                QFile::remove(ini);
                QFile::rename(backup, ini);
            }
        }
    }

    isFactorySettingsTest = QFile::exists(backup);
}

void MW::migrateSortColumnLayout()
{
/*
    ONCE: G::DateColumn was INSERTED after G::CreatedColumn (2026-11-03), so every column
    after it moved up one. A sortColumn is saved as a column NUMBER -- in each workspace
    (the "Workspaces" array and the "WorkflowWorkspaces" groups) and in "LibraryState" --
    and one saved by an older build at or past the insertion point now names the column
    before the one the user chose. Shift those, then set a flag so it never runs twice.
    Columns are matched by header name everywhere else (TableFields), so only sortColumn
    needs this.
*/
    if (G::isLogger) G::log("MW::migrateSortColumnLayout");
    if (!isSettings) return;
    const QString flag = "SortColumnDateInserted";
    if (settings->value(flag, false).toBool()) return;
    for (const QString &key : settings->allKeys()) {
        if (key != "sortColumn" && !key.endsWith("/sortColumn")) continue;
        const int col = settings->value(key).toInt();
        if (col >= G::DateColumn && col + 1 < G::TotalColumns)
            settings->setValue(key, col + 1);
    }
    settings->setValue(flag, true);
}

void MW::removeDeprecatedSettings()
{
    if (G::isLogger) G::log("MW::removeDeprecatedSettings");

    QStringList deprecatedSettings;
    deprecatedSettings
        << "tryConcurrentLoading"
        /* The Develop Edits layout A/B/C picker: the alternatives were dropped and the
           one that stayed is now the only layout, so a remembered value names nothing. */
        << "developEditsLayout"
           ;

    /* The list was being built and then dropped on the floor, so a retired key stayed in
       QSettings for ever. Remove each one for real. */
    for (const QString &key : deprecatedSettings) settings->remove(key);
}
