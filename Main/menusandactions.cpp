#include "Main/mainwindow.h"

void MW::createActions()
{
    if (G::isLogger) G::log("MW::createActions");

    // disable go keys when property editors have focus  rgh chk if req'd
    connect(qApp, &QApplication::focusChanged, this, &MW::focusChange);

    createFileActions();            // File menu
    createEditActions();            // Edit Menu
    createGoActions();              // Go Menu
    createFilterActions();          // Filter Menu
    createSortActions();            // Sort Menu
    createEmbellishActions();       // Embellish Menu
    createViewActions();            // View Menu
    createWindowActions();          // Window Menu
    createHelpActions();            // Help Menu
    createMiscActions();            // Misc
}

void MW::createFileActions()
{
    // File menu
    int n;          // used to populate action lists

    openAction = new QAction(tr("Open Folder..."), this);
    openAction->setObjectName("openFolder");
    openAction->setShortcutVisibleInContextMenu(true);
    addAction(openAction);
    connect(openAction, &QAction::triggered, this, &MW::openFolder);

    refreshCurrentAction = new QAction(tr("Refresh Current Folder"), this);
    refreshCurrentAction->setObjectName("refreshFolder");
    refreshCurrentAction->setShortcutVisibleInContextMenu(true);
    addAction(refreshCurrentAction);
    connect(refreshCurrentAction, &QAction::triggered, this, &MW::refreshCurrentFolder);

    openUsbAction = new QAction(tr("Open Memory Card..."), this);
    openUsbAction->setObjectName("openUsbFolder");
    openUsbAction->setShortcutVisibleInContextMenu(true);
    addAction(openUsbAction);
    connect(openUsbAction, &QAction::triggered, this, &MW::openUsbFolder);

    openWithMenu = new QMenu(tr("Open With..."));

    openWithMenuAction = new QAction(tr("Open With..."), this);
    openWithMenuAction->setObjectName("openWithMenu");
    openWithMenuAction->setShortcutVisibleInContextMenu(true);
    addAction(openWithMenuAction);
    openWithMenuAction->setMenu(openWithMenu);

    manageAppAction = new QAction(tr("Manage External Applications"), this);
    manageAppAction->setObjectName("chooseApp");
    manageAppAction->setShortcutVisibleInContextMenu(true);
    addAction(manageAppAction);
    connect(manageAppAction, &QAction::triggered, this, &MW::externalAppManager);

    /* read external apps from QSettings */
    if (isSettings) {
        settings->beginGroup("ExternalApps");
        QStringList names = settings->childKeys();
        n = names.size();

        for (int i = 0; i < 10; ++i) {
            if (i < n) {
                /* The preferred sort order is prepended to the app names
                in writeSettings as QSettings does not honor order.  When
                loading the settings (here) we have to strip the sort char
                which is defined in the vector xAppShortcut.
                */
                QString name = names.at(i);
                externalApp.name = name.remove(0, 1);
                externalApp.path = settings->value(names.at(i)).toString();
                externalApps.append(externalApp);
            }
            else externalApp.name = "Future app" + QString::number(i);

            appActions.append(new QAction(externalApp.name, this));
            if (i < n) {
                appActions.at(i)->setShortcut(QKeySequence("Alt+" + xAppShortcut[i]));
                appActions.at(i)->setShortcutVisibleInContextMenu(true);
                appActions.at(i)->setText(externalApp.name);
                appActions.at(i)->setVisible(true);
                addAction(appActions.at(i));
            }
            if (i >= n) appActions.at(i)->setVisible(false);
            appActions.at(i)->setShortcut(QKeySequence("Alt+" + xAppShortcut[i]));
            connect(appActions.at(i), &QAction::triggered, this, &MW::runExternalApp);
        }
        addActions(appActions);
        settings->endGroup();

        // app arguments (optional) are in a sister group
        settings->beginGroup("ExternalAppArgs");
        QStringList args = settings->childKeys();
        n = args.size();
        for (int i = 0; i < 10; ++i) {
            if (i < n) {
                QString arg = args.at(i);
                externalApps[i].args = arg.remove(0, 1);
                externalApps[i].args = settings->value(args.at(i)).toString();
            }
        }
        settings->endGroup();
    }
    else {
        for (int i = 0; i < 10; ++i) {
            externalApp.name = "Future app" + QString::number(i);
            appActions.append(new QAction(externalApp.name, this));
            appActions.at(i)->setShortcut(QKeySequence("Alt+" + xAppShortcut[i]));
            connect(appActions.at(i), &QAction::triggered, this, &MW::runExternalApp);
        }
    }

    recentFoldersMenu = new QMenu(tr("Recent folders..."));
    recentFoldersAction = new QAction(tr("Recent folders..."), this);
    recentFoldersAction->setObjectName("recentFoldersAction");
    recentFoldersAction->setShortcutVisibleInContextMenu(true);
    addAction(recentFoldersAction);
    recentFoldersAction->setMenu(recentFoldersMenu);

    // general connection to handle invoking new recent folders
    // MacOS will not allow runtime menu insertions.  Cludge workaround
    // add 20 dummy menu items and then hide until use.
    n = recentFolders->count();
    for (int i = 0; i < maxRecentFolders; i++) {
        QString name;
        QString objName = "";
        if (i < n) {
            name = recentFolders->at(i);
            objName = "recentFolder" + QString::number(i);
        }
        else name = "Future recent folder" + QString::number(i);
        recentFolderActions.append(new QAction(name, this));
        if (i < n) {
            //            recentFolderActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
            recentFolderActions.at(i)->setObjectName(objName);
            recentFolderActions.at(i)->setText(name);
            recentFolderActions.at(i)->setVisible(true);
            addAction(recentFolderActions.at(i));
        }
        if (i >= n) recentFolderActions.at(i)->setVisible(false);
        // see invokeRecentFolder
        //        connect(recentFolderActions.at(i), &QAction::triggered, this, &MW::openFolderFromRecent);
    }
    addActions(recentFolderActions);

    QString revealText = "Reveal";
#ifdef Q_OS_WIN
    revealText = "Open in Explorer";
#endif
#ifdef Q_OS_MAC
    revealText = "Reveal in Finder";
#endif

    saveAsFileAction = new QAction("Save Preview as...", this);
    saveAsFileAction->setObjectName("saveAs");
    saveAsFileAction->setShortcutVisibleInContextMenu(true);
    addAction(saveAsFileAction);
    connect(saveAsFileAction, &QAction::triggered, this, &MW::saveAsFile);

    revealFileAction = new QAction(revealText, this);
    revealFileAction->setObjectName("openInFinder");
    revealFileAction->setShortcutVisibleInContextMenu(true);
    addAction(revealFileAction);
    connect(revealFileAction, &QAction::triggered, this, &MW::revealFile);

    revealFileActionFromContext = new QAction(revealText, this);
    revealFileActionFromContext->setObjectName("openInFinderFromContext");
    revealFileActionFromContext->setShortcutVisibleInContextMenu(true);
    addAction(revealFileActionFromContext);
    connect(revealFileActionFromContext, &QAction::triggered, this, &MW::revealFileFromContext);

    copyFolderPathFromContextAction = new QAction("Copy folder path", this);
    copyFolderPathFromContextAction->setObjectName("copyPathFromContext");
    copyFolderPathFromContextAction->setShortcutVisibleInContextMenu(true);
    addAction(copyFolderPathFromContextAction);
    connect(copyFolderPathFromContextAction, &QAction::triggered, this, &MW::copyFolderPathFromContext);

    copyImagePathFromContextAction = new QAction("Copy path(s)", this);
    copyImagePathFromContextAction->setObjectName("copyPathFromContext");
    copyImagePathFromContextAction->setShortcutVisibleInContextMenu(true);
    addAction(copyImagePathFromContextAction);
    connect(copyImagePathFromContextAction, &QAction::triggered, this, &MW::copyImagePathFromContext);

    renameAction = new QAction(tr("Rename images"), this);
    renameAction->setObjectName("renameFiles");
    renameAction->setShortcutVisibleInContextMenu(true);
    addAction(renameAction);
    connect(renameAction, &QAction::triggered, this, &MW::renameSelectedFiles);

    subFoldersAction = new QAction(tr("Include Subfolders (or shift+ctrl click)  "), this);
    subFoldersAction->setObjectName("subFolders");
    subFoldersAction->setShortcutVisibleInContextMenu(true);
    subFoldersAction->setCheckable(true);
    subFoldersAction->setChecked(false);
    addAction(subFoldersAction);
    connect(subFoldersAction, &QAction::triggered, this, &MW::updateStatusBar);

    refreshFoldersAction = new QAction(tr("Refresh folders"), this);
    refreshFoldersAction->setObjectName("refreshFolders");
    refreshFoldersAction->setShortcutVisibleInContextMenu(true);
    addAction(refreshFoldersAction);
    connect(refreshFoldersAction, &QAction::triggered, this, &MW::refreshFolders);

    collapseFoldersAction = new QAction(tr("Collapse all folders"), this);
    collapseFoldersAction->setObjectName("collapseFolders");
    collapseFoldersAction->setShortcutVisibleInContextMenu(true);
    addAction(collapseFoldersAction);
    connect(collapseFoldersAction, &QAction::triggered, this, &MW::collapseAllFolders);

    showImageCountAction = new QAction(tr("Show image count"), this);
    showImageCountAction->setObjectName("showImageCount");
    showImageCountAction->setShortcutVisibleInContextMenu(true);
    showImageCountAction->setCheckable(true);
    //    showImageCountAction->setChecked(setting->value("showImageCount").toBool());
    addAction(showImageCountAction);
    connect(showImageCountAction, &QAction::triggered, this, &MW::setShowImageCount);

    addBookmarkAction = new QAction(tr("Add Bookmark"), this);
    addBookmarkAction->setObjectName("addBookmark");
    addBookmarkAction->setShortcutVisibleInContextMenu(true);
    addAction(addBookmarkAction);
    connect(addBookmarkAction, &QAction::triggered, this, &MW::addNewBookmarkFromMenu);

    addBookmarkActionFromContext = new QAction(tr("Add Bookmark"), this);
    addBookmarkActionFromContext->setObjectName("addBookmark");
    addBookmarkActionFromContext->setShortcutVisibleInContextMenu(true);
    addAction(addBookmarkActionFromContext);
    connect(addBookmarkActionFromContext, &QAction::triggered, this, &MW::addNewBookmarkFromContextMenu);

    removeBookmarkAction = new QAction(tr("Remove Bookmark"), this);
    removeBookmarkAction->setObjectName("removeBookmark");
    removeBookmarkAction->setShortcutVisibleInContextMenu(true);
    addAction(removeBookmarkAction);
    connect(removeBookmarkAction, &QAction::triggered, this, &MW::removeBookmark);

    refreshBookmarkAction = new QAction(tr("Refresh Bookmarks"), this);
    refreshBookmarkAction->setObjectName("refreshBookmarks");
    refreshBookmarkAction->setShortcutVisibleInContextMenu(true);
    addAction(refreshBookmarkAction);
    connect(refreshBookmarkAction, &QAction::triggered, this, &MW::refreshBookmarks);

    ingestAction = new QAction(tr("Ingest picks"), this);
    ingestAction->setObjectName("ingest");
    ingestAction->setShortcutVisibleInContextMenu(true);
    addAction(ingestAction);
    connect(ingestAction, &QAction::triggered, this, &MW::ingest);

    ingestHistoryFoldersMenu = new QMenu(tr("Ingest History folders..."));
    ingestHistoryFoldersAction = new QAction(tr("Ingest History..."), this);
    ingestHistoryFoldersAction->setObjectName("ingestHistoryFoldersAction");
    ingestHistoryFoldersAction->setShortcutVisibleInContextMenu(true);
    addAction(ingestHistoryFoldersAction);
    ingestHistoryFoldersAction->setMenu(ingestHistoryFoldersMenu);

    // general connection to add ingest history list as menu items
    // MacOS will not allow runtime menu insertions.  Cludge workaround
    // add 20 dummy menu items and then hide until use.
    n = ingestHistoryFolders->count();
    for (int i = 0; i < maxIngestHistoryFolders; i++) {
        QString name;
        QString objName = "";
        if (i < n) {
            name = ingestHistoryFolders->at(i);
            objName = "ingestHistoryFolder" + QString::number(i);
        }
        else name = "Future ingest history folder" + QString::number(i);
        ingestHistoryFolderActions.append(new QAction(name, this));
        if (i < n) {
            ingestHistoryFolderActions.at(i)->setObjectName(objName);
            ingestHistoryFolderActions.at(i)->setText(name);
            ingestHistoryFolderActions.at(i)->setVisible(true);
            addAction(ingestHistoryFolderActions.at(i));
        }
        if (i >= n) ingestHistoryFolderActions.at(i)->setVisible(false);
    }
    addActions(ingestHistoryFolderActions);

    ejectAction = new QAction(tr("Eject Memory Card"), this);
    ejectAction->setObjectName("ejectUsbDrive");
    ejectAction->setShortcutVisibleInContextMenu(true);
    addAction(ejectAction);
    connect(ejectAction, &QAction::triggered, this, &MW::ejectUsbFromMainMenu);

    ejectActionFromContextMenu = new QAction(tr("Eject Memory Card"), this);
    ejectActionFromContextMenu->setObjectName("ejectUsbDriveFromContext");
    ejectActionFromContextMenu->setShortcutVisibleInContextMenu(true);
    addAction(ejectActionFromContextMenu);
    connect(ejectActionFromContextMenu, &QAction::triggered, this, &MW::ejectUsbFromContextMenu);

    eraseUsbAction = new QAction(tr("Erase Memory Card Images..."), this);
    eraseUsbAction->setObjectName("eraseUsbFolder");
    eraseUsbAction->setShortcutVisibleInContextMenu(true);
    addAction(eraseUsbAction);
    connect(eraseUsbAction, &QAction::triggered, this, &MW::eraseMemCardImages);

    eraseUsbActionFromContextMenu = new QAction(tr("Erase Memory Card Images"), this);
    eraseUsbActionFromContextMenu->setObjectName("eraseUsbFolderFromContext");
    eraseUsbActionFromContextMenu->setShortcutVisibleInContextMenu(true);
    addAction(eraseUsbActionFromContextMenu);
    connect(eraseUsbActionFromContextMenu, &QAction::triggered, this, &MW::eraseMemCardImagesFromContextMenu);

    colorManageAction = new QAction(tr("Color manage"), this);
    colorManageAction->setObjectName("colorManage");
    colorManageAction->setShortcutVisibleInContextMenu(true);
    colorManageAction->setCheckable(true);
    colorManageAction->setChecked(G::colorManage);
    addAction(colorManageAction);
    connect(colorManageAction, &QAction::triggered, this, &MW::toggleColorManageClick);

    combineRawJpgAction = new QAction(tr("Combine Raw+Jpg"), this);
    combineRawJpgAction->setObjectName("combineRawJpg");
    combineRawJpgAction->setShortcutVisibleInContextMenu(true);
    combineRawJpgAction->setCheckable(true);
    if (isSettings && settings->contains("combineRawJpg")) combineRawJpgAction->setChecked(settings->value("combineRawJpg").toBool());
    else combineRawJpgAction->setChecked(false);
    addAction(combineRawJpgAction);
    connect(combineRawJpgAction, &QAction::triggered, this, &MW::setCombineRawJpg);

    // Place keeper for now
    runDropletAction = new QAction(tr("Run Droplet"), this);
    runDropletAction->setObjectName("runDroplet");
    runDropletAction->setShortcutVisibleInContextMenu(true);
    runDropletAction->setShortcut(QKeySequence("A"));
    addAction(runDropletAction);

    reportMetadataAction = new QAction(tr("Report Metadata"), this);
    reportMetadataAction->setObjectName("reportMetadata");
    reportMetadataAction->setShortcutVisibleInContextMenu(true);
    addAction(reportMetadataAction);
    connect(reportMetadataAction, &QAction::triggered, this, &MW::reportMetadata);

    // Appears in Winnow menu in OSX
    exitAction = new QAction(tr("Exit"), this);
    exitAction->setObjectName("exit");
    exitAction->setShortcutVisibleInContextMenu(true);
    addAction(exitAction);
    connect(exitAction, &QAction::triggered, this, &MW::close);
}

void MW::createEditActions()
{
    // Edit Menu

    selectAllAction = new QAction(tr("Select All"), this);
    selectAllAction->setObjectName("selectAll");
    selectAllAction->setShortcutVisibleInContextMenu(true);
    addAction(selectAllAction);
    connect(selectAllAction, &QAction::triggered, this, &MW::selectAllThumbs);

    invertSelectionAction = new QAction(tr("Invert Selection"), this);
    invertSelectionAction->setObjectName("invertSelection");
    invertSelectionAction->setShortcutVisibleInContextMenu(true);
    addAction(invertSelectionAction);
    connect(invertSelectionAction, &QAction::triggered, sel, &Selection::invert);
    //    connect(invertSelectionAction, &QAction::triggered,
    //            thumbView, &IconView::invertSelection);

    rejectAction = new QAction(tr("Reject"), this);
    rejectAction->setObjectName("Reject");
    rejectAction->setShortcutVisibleInContextMenu(true);
    addAction(rejectAction);
    connect(rejectAction, &QAction::triggered, this, &MW::toggleReject);

    pickAction = new QAction(tr("Pick"), this);
    pickAction->setObjectName("togglePick");
    pickAction->setShortcutVisibleInContextMenu(true);
    addAction(pickAction);
    connect(pickAction, &QAction::triggered, this, &MW::togglePick);

    pick1Action = new QAction(tr("Pick"), this);  // added for shortcut "P"
    addAction(pick1Action);
    connect(pick1Action, &QAction::triggered, this, &MW::togglePick);

    pickMouseOverAction = new QAction(tr("Pick"), this);  // IconView context menu
    pickMouseOverAction->setObjectName("toggleMouseOverPick");
    pickAction->setShortcutVisibleInContextMenu(true);
    addAction(pickMouseOverAction);
    connect(pickMouseOverAction, &QAction::triggered, this, &MW::togglePick);
    //    connect(pickMouseOverAction, &QAction::triggered, this, &MW::togglePickMouseOver);

    pickUnlessRejectedAction = new QAction(tr("Pick unless rejected"), this);
    pickUnlessRejectedAction->setObjectName("pickUnlessRejected");
    pickUnlessRejectedAction->setShortcutVisibleInContextMenu(true);
    addAction(pickUnlessRejectedAction);
    connect(pickUnlessRejectedAction, &QAction::triggered, this, &MW::togglePickUnlessRejected);

    filterPickAction = new QAction(tr("Filter picks only"), this);
    filterPickAction->setObjectName("toggleFilterPick");
    filterPickAction->setShortcutVisibleInContextMenu(true);
    filterPickAction->setCheckable(true);
    filterPickAction->setChecked(false);
    addAction(filterPickAction);
    // lamda example
    connect(filterPickAction, &QAction::triggered, filters,
            [this](){filters->setPicksState(filterPickAction->isChecked());});

    popPickHistoryAction = new QAction(tr("Recover prior pick state"), this);
    popPickHistoryAction->setObjectName("togglePick");
    popPickHistoryAction->setShortcutVisibleInContextMenu(true);
    addAction(popPickHistoryAction);
    connect(popPickHistoryAction, &QAction::triggered, this, &MW::popPick);

    //
    QString moveFilesToWhatever;
    QString moveFolderToWhatever;
    #ifdef Q_OS_WIN
    moveFilesToWhatever = "Move file(s) to recycle bin";
    moveFolderToWhatever = "Move folder to recycle bin";
    #endif
    #ifdef Q_OS_MAC
    moveFilesToWhatever = "Move file(s) to trash";
    moveFolderToWhatever = "Move folder to trash";
    #endif
    deleteImagesAction = new QAction(moveFilesToWhatever, this);
    deleteImagesAction->setObjectName("deleteFiles");
    deleteImagesAction->setShortcutVisibleInContextMenu(true);
    deleteImagesAction->setShortcut(QKeySequence("Delete"));
    addAction(deleteImagesAction);
    connect(deleteImagesAction, &QAction::triggered, this, &MW::deleteSelectedFiles);

    deleteAction1 = new QAction(moveFilesToWhatever, this);
    deleteAction1->setObjectName("backspaceDeleteFiles");
    deleteAction1->setShortcutVisibleInContextMenu(true);
    deleteAction1->setShortcut(QKeySequence("Backspace"));
    addAction(deleteAction1);
    connect(deleteAction1, &QAction::triggered, this, &MW::deleteSelectedFiles);

    deleteActiveFolderAction = new QAction(moveFolderToWhatever, this);
    deleteActiveFolderAction->setObjectName("deleteActiveFolder");
    addAction(deleteActiveFolderAction);
    connect(deleteActiveFolderAction, &QAction::triggered, this, &MW::deleteFolder);

    // not being used due to risk of folder containing many subfolders with no indication to user
    deleteBookmarkFolderAction = new QAction(moveFolderToWhatever, this);
    deleteBookmarkFolderAction->setObjectName("deleteBookmarkFolder");
    addAction(deleteBookmarkFolderAction);
    connect(deleteBookmarkFolderAction, &QAction::triggered, this, &MW::deleteFolder);

    deleteFSTreeFolderAction = new QAction(moveFolderToWhatever, this);
    deleteFSTreeFolderAction->setObjectName("deleteFSTreeFolder");
    addAction(deleteFSTreeFolderAction);
    connect(deleteFSTreeFolderAction, &QAction::triggered, this, &MW::deleteFolder);

    shareFilesAction = new QAction(tr("Share..."), this);
    shareFilesAction->setObjectName("shareFiles");
    shareFilesAction->setShortcutVisibleInContextMenu(true);
    addAction(shareFilesAction);
    connect(shareFilesAction, &QAction::triggered, this, &MW::shareFiles);

    copyFilesAction = new QAction(tr("Copy files"), this);
    copyFilesAction->setObjectName("copyFiles");
    copyFilesAction->setShortcutVisibleInContextMenu(true);
    copyFilesAction->setShortcut(QKeySequence("Ctrl+C"));
    addAction(copyFilesAction);
    connect(copyFilesAction, &QAction::triggered, this, &MW::copyFiles);

    copyImageAction = new QAction(tr("Copy images"), this);
    copyImageAction->setObjectName("copyImage");
    copyImageAction->setShortcutVisibleInContextMenu(true);
    copyImageAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    addAction(copyImageAction);
    connect(copyImageAction, &QAction::triggered, imageView, &ImageView::copyImage);

    searchTextEditAction = new QAction(tr("Search for..."), this);
    searchTextEditAction->setObjectName("searchTextEdit");
    searchTextEditAction->setShortcutVisibleInContextMenu(true);
    addAction(searchTextEditAction);
    connect(searchTextEditAction, &QAction::triggered, this, &MW::searchTextEdit);

    rate0Action = new QAction(tr("Clear rating"), this);
    rate0Action->setObjectName("Rate0");
    rate0Action->setShortcutVisibleInContextMenu(true);
    addAction(rate0Action);
    connect(rate0Action, &QAction::triggered, this, &MW::setRating);

    rate1Action = new QAction(tr("Set rating to 1"), this);
    rate1Action->setObjectName("Rate1");
    rate1Action->setShortcutVisibleInContextMenu(true);
    addAction(rate1Action);
    connect(rate1Action, &QAction::triggered, this, &MW::setRating);

    rate2Action = new QAction(tr("Set rating to 2"), this);
    rate2Action->setObjectName("Rate2");
    rate2Action->setShortcutVisibleInContextMenu(true);
    addAction(rate2Action);
    connect(rate2Action, &QAction::triggered, this, &MW::setRating);

    rate3Action = new QAction(tr("Set rating to 3"), this);
    rate3Action->setObjectName("Rate3");
    rate3Action->setShortcutVisibleInContextMenu(true);
    addAction(rate3Action);
    connect(rate3Action, &QAction::triggered, this, &MW::setRating);

    rate4Action = new QAction(tr("Set rating to 4"), this);
    rate4Action->setObjectName("Rate4");
    rate4Action->setShortcutVisibleInContextMenu(true);
    addAction(rate4Action);
    connect(rate4Action, &QAction::triggered, this, &MW::setRating);

    rate5Action = new QAction(tr("Set rating to 5"), this);
    rate5Action->setObjectName("Rate5");
    rate5Action->setShortcutVisibleInContextMenu(true);
    addAction(rate5Action);
    connect(rate5Action, &QAction::triggered, this, &MW::setRating);

    label0Action = new QAction(tr("Clear colour"), this);
    label0Action->setObjectName("Label0");
    label0Action->setShortcutVisibleInContextMenu(true);
    addAction(label0Action);
    connect(label0Action, &QAction::triggered, this, &MW::setColorClass);

    label1Action = new QAction(tr("Set to Red"), this);
    label1Action->setObjectName("Label1");
    label1Action->setShortcutVisibleInContextMenu(true);
    addAction(label1Action);
    connect(label1Action, &QAction::triggered, this, &MW::setColorClass);

    label2Action = new QAction(tr("Set to Yellow"), this);
    label2Action->setObjectName("Label2");
    label2Action->setShortcutVisibleInContextMenu(true);
    addAction(label2Action);
    connect(label2Action, &QAction::triggered, this, &MW::setColorClass);

    label3Action = new QAction(tr("Set to Green"), this);
    label3Action->setObjectName("Label3");
    label3Action->setShortcutVisibleInContextMenu(true);
    addAction(label3Action);
    connect(label3Action, &QAction::triggered, this, &MW::setColorClass);

    label4Action = new QAction(tr("Set to Blue"), this);
    label4Action->setObjectName("Label4");
    label4Action->setShortcutVisibleInContextMenu(true);
    addAction(label4Action);
    connect(label4Action, &QAction::triggered, this, &MW::setColorClass);

    label5Action = new QAction(tr("Set to Purple"), this);
    label5Action->setObjectName("Label5");
    label5Action->setShortcutVisibleInContextMenu(true);
    addAction(label5Action);
    connect(label5Action, &QAction::triggered, this, &MW::setColorClass);

    embedThumbnailsAction = new QAction(tr("Embed missing thumbnails  "), this);
    embedThumbnailsAction->setObjectName("embedThumbnails");
    embedThumbnailsAction->setShortcutVisibleInContextMenu(true);
    addAction(embedThumbnailsAction);
    connect(embedThumbnailsAction, &QAction::triggered, this, &MW::embedThumbnailsFromAction);

    rotateRightAction = new QAction(tr("Rotate CW"), this);
    rotateRightAction->setObjectName("rotateRight");
    rotateRightAction->setShortcutVisibleInContextMenu(true);
    addAction(rotateRightAction);
    connect(rotateRightAction, &QAction::triggered, this, &MW::rotateRight);

    rotateLeftAction = new QAction(tr("Rotate CCW"), this);
    rotateLeftAction->setObjectName("rotateLeft");
    rotateLeftAction->setShortcutVisibleInContextMenu(true);
    addAction(rotateLeftAction);
    connect(rotateLeftAction, &QAction::triggered, this, &MW::rotateLeft);

    // Utilities
    mediaReadSpeedAction = new QAction(tr("Media read speed"), this);
    mediaReadSpeedAction->setObjectName("mediaReadSpeed");
    mediaReadSpeedAction->setShortcutVisibleInContextMenu(true);
    addAction(mediaReadSpeedAction);
    connect(mediaReadSpeedAction, &QAction::triggered, this, &MW::mediaReadSpeed);

    visCmpImagesAction = new QAction(tr("Find duplicates"), this);
    visCmpImagesAction->setObjectName("visCmpImages");
    visCmpImagesAction->setShortcutVisibleInContextMenu(true);
    addAction(visCmpImagesAction);
    connect(visCmpImagesAction, &QAction::triggered, this, &MW::findDuplicates);

    reportHueCountAction = new QAction(tr("Report hue count"), this);
    reportHueCountAction->setObjectName("reportHueCount");
    reportHueCountAction->setShortcutVisibleInContextMenu(true);
    addAction(reportHueCountAction);
    connect(reportHueCountAction, &QAction::triggered, this, &MW::reportHueCount);

    meanStackAction = new QAction(tr("Mean stack"), this);
    meanStackAction->setObjectName("meanStack");
    meanStackAction->setShortcutVisibleInContextMenu(true);
    addAction(meanStackAction);
    connect(meanStackAction, &QAction::triggered, this, &MW::generateMeanStack);
    // End Utilities

    prefAction = new QAction(tr("Preferences"), this);
    prefAction->setObjectName("settings");
    prefAction->setShortcutVisibleInContextMenu(true);
    addAction(prefAction);
    connect(prefAction, &QAction::triggered, [this]() {preferences("");} );

    prefInfoAction = new QAction(tr("Hide or show info fields"), this);
    prefInfoAction->setObjectName("infosettings");
    prefInfoAction->setShortcutVisibleInContextMenu(true);
    addAction(prefInfoAction);
    connect(prefInfoAction, &QAction::triggered, this, &MW::infoViewPreferences);
}

void MW::createGoActions()
{
    // Go menu

    //    keyRightAction = new QAction(tr("Next"), this);
    //    keyRightAction->setObjectName("nextImage");
    //    keyRightAction->setShortcutVisibleInContextMenu(true);
    //    keyRightAction->setEnabled(true);
    //    addAction(keyRightAction);
    //    connect(keyRightAction, &QAction::triggered, this, [this](){ keyRight(Qt::NoModifier); });

    //    keyLeftAction = new QAction(tr("Previous"), this);
    //    keyLeftAction->setObjectName("prevImage");
    //    keyLeftAction->setShortcutVisibleInContextMenu(true);
    //    addAction(keyLeftAction);
    //    connect(keyLeftAction, &QAction::triggered, this, &MW::keyLeft);

    //    keyUpAction = new QAction(tr("Up"), this);
    //    keyUpAction->setObjectName("moveUp");
    //    keyUpAction->setShortcutVisibleInContextMenu(true);
    //    addAction(keyUpAction);
    //    connect(keyUpAction, &QAction::triggered, this, &MW::keyUp);

    //    keyDownAction = new QAction(tr("Down"), this);
    //    keyDownAction->setObjectName("moveDown");
    //    keyDownAction->setShortcutVisibleInContextMenu(true);
    //    addAction(keyDownAction);
    //    connect(keyDownAction, &QAction::triggered, this, &MW::keyDown);

    //    keyPageUpAction = new QAction(tr("Page Up"), this);
    //    keyPageUpAction->setObjectName("movePageUp");
    //    keyPageUpAction->setShortcutVisibleInContextMenu(true);
    //    addAction(keyPageUpAction);
    //    connect(keyPageUpAction, &QAction::triggered, this, &MW::keyPageUp);

    //    keyPageDownAction = new QAction(tr("Page Down"), this);
    //    keyPageDownAction->setObjectName("movePageDown");
    //    keyPageDownAction->setShortcutVisibleInContextMenu(true);
    //    addAction(keyPageDownAction);
    //    connect(keyPageDownAction, &QAction::triggered, this, &MW::keyPageDown);

    //    keyHomeAction = new QAction(tr("First"), this);
    //    keyHomeAction->setObjectName("firstImage");
    //    keyHomeAction->setShortcutVisibleInContextMenu(true);
    //    addAction(keyHomeAction);
    //    connect(keyHomeAction, &QAction::triggered, this, &MW::keyHome);

    //    keyEndAction = new QAction(tr("Last"), this);
    //    keyEndAction->setObjectName("lastImage");
    //    keyEndAction->setShortcutVisibleInContextMenu(true);
    //    addAction(keyEndAction);
    //    connect(keyEndAction, &QAction::triggered, this, &MW::keyEnd);

    jumpAction = new QAction(tr("Jump to Row"), this);
    jumpAction->setObjectName("jump");
    jumpAction->setShortcutVisibleInContextMenu(true);
    addAction(jumpAction);
    connect(jumpAction, &QAction::triggered, this, &MW::jump);

    keyScrollLeftAction = new QAction(tr("Scroll Left"), this);
    keyScrollLeftAction->setObjectName("scrollLeft");
    keyScrollLeftAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollLeftAction);
    connect(keyScrollLeftAction, &QAction::triggered, this, &MW::keyScrollUp);

    keyScrollRightAction = new QAction(tr("Scroll Right"), this);
    keyScrollRightAction->setObjectName("scrollRight");
    keyScrollRightAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollRightAction);
    connect(keyScrollRightAction, &QAction::triggered, this, &MW::keyScrollDown);

    keyScrollUpAction = new QAction(tr("Scroll Up"), this);
    keyScrollUpAction->setObjectName("scrollUp");
    keyScrollUpAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollUpAction);
    connect(keyScrollUpAction, &QAction::triggered, this, &MW::keyScrollUp);

    keyScrollDownAction = new QAction(tr("Scroll Down"), this);
    keyScrollDownAction->setObjectName("scrollDown");
    keyScrollDownAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollDownAction);
    connect(keyScrollDownAction, &QAction::triggered, this, &MW::keyScrollDown);

    keyScrollPageLeftAction = new QAction(tr("Scroll Page Left"), this);
    keyScrollPageLeftAction->setObjectName("scrollPageLeft");
    keyScrollPageLeftAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollPageLeftAction);
    connect(keyScrollPageLeftAction, &QAction::triggered, this, &MW::keyScrollPageUp);

    keyScrollPageRightAction = new QAction(tr("Scroll Page Right"), this);
    keyScrollPageRightAction->setObjectName("scrollPageRight");
    keyScrollPageRightAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollPageRightAction);
    connect(keyScrollPageRightAction, &QAction::triggered, this, &MW::keyScrollPageDown);

    keyScrollPageUpAction = new QAction(tr("Scroll Page up"), this);
    keyScrollPageUpAction->setObjectName("scrollPageUp");
    keyScrollPageUpAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollPageUpAction);
    connect(keyScrollPageUpAction, &QAction::triggered, this, &MW::keyScrollPageUp);

    keyScrollPageDownAction = new QAction(tr("Scroll Page down"), this);
    keyScrollPageDownAction->setObjectName("scrollPageDown");
    keyScrollPageDownAction->setShortcutVisibleInContextMenu(true);
    addAction(keyScrollPageDownAction);
    connect(keyScrollPageDownAction, &QAction::triggered, this, &MW::keyScrollPageDown);

    // Not a menu item - used by slide show
    randomImageAction = new QAction(tr("Random"), this);
    randomImageAction->setObjectName("randomImage");
    randomImageAction->setShortcutVisibleInContextMenu(true);
    addAction(randomImageAction);
    connect(randomImageAction, &QAction::triggered, sel, &Selection::random);
    //    connect(randomImageAction, &QAction::triggered, thumbView, &IconView::selectRandom);

    nextPickAction = new QAction(tr("Next Pick"), this);
    nextPickAction->setObjectName("nextPick");
    nextPickAction->setShortcutVisibleInContextMenu(true);
    addAction(nextPickAction);
    connect(nextPickAction, &QAction::triggered, sel, [this](){ sel->nextPick(Qt::NoModifier); });

    prevPickAction = new QAction(tr("Previous Pick"), this);
    prevPickAction->setObjectName("prevPick");
    prevPickAction->setShortcutVisibleInContextMenu(true);
    addAction(prevPickAction);
    connect(prevPickAction, &QAction::triggered, sel, [this](){ sel->prevPick(Qt::NoModifier); });
}

void MW::createFilterActions()
{
    // Filters

    clearAllFiltersAction = new QAction(tr("Clear all filters"), this);
    clearAllFiltersAction->setObjectName("uncheckAllFilters");
    clearAllFiltersAction->setShortcutVisibleInContextMenu(true);
    addAction(clearAllFiltersAction);
    connect(clearAllFiltersAction, &QAction::triggered, this, &MW::clearAllFilters);

    expandAllFiltersAction = new QAction(tr("Expand all filters"), this);
    expandAllFiltersAction->setObjectName("expandAll");
    expandAllFiltersAction->setShortcutVisibleInContextMenu(true);
    addAction(expandAllFiltersAction);
    connect(expandAllFiltersAction, &QAction::triggered, filters, &Filters::expandAllFilters);

    collapseAllFiltersAction = new QAction(tr("Collapse all filters"), this);
    collapseAllFiltersAction->setObjectName("collapseAll");
    collapseAllFiltersAction->setShortcutVisibleInContextMenu(true);
    addAction(collapseAllFiltersAction);
    connect(collapseAllFiltersAction, &QAction::triggered, filters, &Filters::collapseAllFilters);

    filterSearchAction = new QAction(tr("Filter by search text"), this);
    filterSearchAction->setCheckable(true);
    filterSearchAction->setShortcutVisibleInContextMenu(true);
    addAction(filterSearchAction);
    connect(filterSearchAction, &QAction::triggered, this, &MW::quickFilter);

    filterRating1Action = new QAction(tr("Filter by rating 1"), this);
    filterRating1Action->setCheckable(true);
    filterRating1Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating1Action);
    connect(filterRating1Action, &QAction::triggered, this, &MW::quickFilter);

    filterRating2Action = new QAction(tr("Filter by rating 2"), this);
    filterRating2Action->setCheckable(true);
    filterRating2Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating2Action);
    connect(filterRating2Action,  &QAction::triggered, this, &MW::quickFilter);

    filterRating3Action = new QAction(tr("Filter by rating 3"), this);
    filterRating3Action->setCheckable(true);
    filterRating3Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating3Action);
    connect(filterRating3Action,  &QAction::triggered, this, &MW::quickFilter);

    filterRating4Action = new QAction(tr("Filter by rating 4"), this);
    filterRating4Action->setCheckable(true);
    filterRating4Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating4Action);
    connect(filterRating4Action,  &QAction::triggered, this, &MW::quickFilter);

    filterRating5Action = new QAction(tr("Filter by rating 5"), this);
    filterRating5Action->setCheckable(true);
    filterRating5Action->setShortcutVisibleInContextMenu(true);
    addAction(filterRating5Action);
    connect(filterRating5Action,  &QAction::triggered, this, &MW::quickFilter);

    filterRedAction = new QAction(tr("Filter by Red"), this);
    filterRedAction->setShortcutVisibleInContextMenu(true);
    filterRedAction->setCheckable(true);
    addAction(filterRedAction);
    connect(filterRedAction,  &QAction::triggered, this, &MW::quickFilter);

    filterYellowAction = new QAction(tr("Filter by Yellow"), this);
    filterYellowAction->setShortcutVisibleInContextMenu(true);
    filterYellowAction->setCheckable(true);
    addAction(filterYellowAction);
    connect(filterYellowAction,  &QAction::triggered, this, &MW::quickFilter);

    filterGreenAction = new QAction(tr("Filter by Green"), this);
    filterGreenAction->setShortcutVisibleInContextMenu(true);
    filterGreenAction->setCheckable(true);
    addAction(filterGreenAction);
    connect(filterGreenAction,  &QAction::triggered, this, &MW::quickFilter);

    filterBlueAction = new QAction(tr("Filter by Blue"), this);
    filterBlueAction->setShortcutVisibleInContextMenu(true);
    filterBlueAction->setCheckable(true);
    addAction(filterBlueAction);
    connect(filterBlueAction,  &QAction::triggered, this, &MW::quickFilter);

    filterPurpleAction = new QAction(tr("Filter by Purple"), this);
    filterPurpleAction->setShortcutVisibleInContextMenu(true);
    filterPurpleAction->setCheckable(true);
    addAction(filterPurpleAction);
    connect(filterPurpleAction,  &QAction::triggered, this, &MW::quickFilter);

    filterInvertAction = new QAction(tr("Invert Filter"), this);
    filterInvertAction->setShortcutVisibleInContextMenu(true);
    filterInvertAction->setCheckable(false);
    addAction(filterInvertAction);
    connect(filterInvertAction,  &QAction::triggered, this, &MW::invertFilters);

    filterLastDayAction = new QAction(tr("Most recent day"), this);
    filterLastDayAction->setShortcutVisibleInContextMenu(true);
    filterLastDayAction->setCheckable(true);
    addAction(filterLastDayAction);
    connect(filterLastDayAction,  &QAction::triggered, this, &MW::filterLastDay);

    filterUpdateAction = new QAction(tr("Update all filters"), this);
    filterUpdateAction->setShortcutVisibleInContextMenu(true);
    addAction(filterUpdateAction);
    connect(filterUpdateAction,  &QAction::triggered, this, &MW::updateAllFilters);

    filterSoloAction = new QAction(tr("Solo mode"), this);
    filterSoloAction->setShortcutVisibleInContextMenu(true);
    filterSoloAction->setCheckable(true);
    addAction(filterSoloAction);
    if (!settings->value("isSoloFilters").isValid() || simulateJustInstalled)
        filterSoloAction->setChecked(true);
    else
        filterSoloAction->setChecked(settings->value("isSoloFilters").toBool());
    setFilterSolo();    // set solo in filters and save to settings
    connect(filterSoloAction,  &QAction::triggered, this, &MW::setFilterSolo);
}

void MW::createSortActions()
{
    // Sort Menu

    sortFileNameAction = new QAction(tr("Sort by file name"), this);
    sortFileNameAction->setShortcutVisibleInContextMenu(true);
    sortFileNameAction->setCheckable(true);
    addAction(sortFileNameAction);
    connect(sortFileNameAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortFileTypeAction = new QAction(tr("Sort by file type"), this);
    sortFileTypeAction->setShortcutVisibleInContextMenu(true);
    sortFileTypeAction->setCheckable(true);
    addAction(sortFileTypeAction);
    connect(sortFileTypeAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortFileSizeAction = new QAction(tr("Sort by file size"), this);
    sortFileSizeAction->setShortcutVisibleInContextMenu(true);
    sortFileSizeAction->setCheckable(true);
    addAction(sortFileSizeAction);
    connect(sortFileSizeAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortCreateAction = new QAction(tr("Sort by created time"), this);
    sortCreateAction->setShortcutVisibleInContextMenu(true);
    sortCreateAction->setCheckable(true);
    addAction(sortCreateAction);
    connect(sortCreateAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortModifyAction = new QAction(tr("Sort by last modified time"), this);
    sortModifyAction->setShortcutVisibleInContextMenu(true);
    sortModifyAction->setCheckable(true);
    addAction(sortModifyAction);
    connect(sortModifyAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortPickAction = new QAction(tr("Sort by picked status"), this);
    sortPickAction->setShortcutVisibleInContextMenu(true);
    sortPickAction->setCheckable(true);
    addAction(sortPickAction);
    connect(sortPickAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortRatingAction = new QAction(tr("Sort by rating"), this);
    sortRatingAction->setShortcutVisibleInContextMenu(true);
    sortRatingAction->setCheckable(true);
    addAction(sortRatingAction);
    connect(sortRatingAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortLabelAction = new QAction(tr("Sort by color label"), this);
    sortLabelAction->setShortcutVisibleInContextMenu(true);
    sortLabelAction->setCheckable(true);
    addAction(sortLabelAction);
    connect(sortLabelAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortMegaPixelsAction = new QAction(tr("Sort by megapixels"), this);
    sortMegaPixelsAction->setShortcutVisibleInContextMenu(true);
    sortMegaPixelsAction->setCheckable(true);
    addAction(sortMegaPixelsAction);
    connect(sortMegaPixelsAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortDimensionsAction = new QAction(tr("Sort by dimensions"), this);
    sortDimensionsAction->setShortcutVisibleInContextMenu(true);
    sortDimensionsAction->setCheckable(true);
    addAction(sortDimensionsAction);
    connect(sortDimensionsAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortApertureAction = new QAction(tr("Sort by aperture"), this);
    //    sortApertureAction->setObjectName("SortAperture");
    sortApertureAction->setShortcutVisibleInContextMenu(true);
    sortApertureAction->setCheckable(true);
    addAction(sortApertureAction);
    connect(sortApertureAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortShutterSpeedAction = new QAction(tr("Sort by shutter speed"), this);
    sortShutterSpeedAction->setShortcutVisibleInContextMenu(true);
    sortShutterSpeedAction->setCheckable(true);
    addAction(sortShutterSpeedAction);
    connect(sortShutterSpeedAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortISOAction = new QAction(tr("Sort by ISO"), this);
    sortISOAction->setShortcutVisibleInContextMenu(true);
    sortISOAction->setCheckable(true);
    addAction(sortISOAction);
    connect(sortISOAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortModelAction = new QAction(tr("Sort by camera model"), this);
    sortModelAction->setShortcutVisibleInContextMenu(true);
    sortModelAction->setCheckable(true);
    addAction(sortModelAction);
    connect(sortModelAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortFocalLengthAction = new QAction(tr("Sort by focal length"), this);
    sortFocalLengthAction->setShortcutVisibleInContextMenu(true);
    sortFocalLengthAction->setCheckable(true);
    addAction(sortFocalLengthAction);
    connect(sortFocalLengthAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortTitleAction = new QAction(tr("Sort by title"), this);
    sortTitleAction->setShortcutVisibleInContextMenu(true);
    sortTitleAction->setCheckable(true);
    addAction(sortTitleAction);
    connect(sortTitleAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortLensAction = new QAction(tr("Sort by lens"), this);
    sortLensAction->setShortcutVisibleInContextMenu(true);
    sortLensAction->setCheckable(true);
    addAction(sortLensAction);
    connect(sortLensAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortCreatorAction = new QAction(tr("Sort by creator"), this);
    sortCreatorAction->setShortcutVisibleInContextMenu(true);
    sortCreatorAction->setCheckable(true);
    addAction(sortCreatorAction);
    connect(sortCreatorAction, &QAction::triggered, this, &MW::sortChangeFromAction);

    sortGroupAction = new QActionGroup(this);
    sortGroupAction->setExclusive(true);

    sortGroupAction->addAction(sortFileNameAction);
    sortGroupAction->addAction(sortFileTypeAction);
    sortGroupAction->addAction(sortFileSizeAction);
    sortGroupAction->addAction(sortCreateAction);
    sortGroupAction->addAction(sortModifyAction);
    sortGroupAction->addAction(sortPickAction);
    sortGroupAction->addAction(sortLabelAction);
    sortGroupAction->addAction(sortRatingAction);
    sortGroupAction->addAction(sortMegaPixelsAction);
    sortGroupAction->addAction(sortDimensionsAction);
    sortGroupAction->addAction(sortApertureAction);
    sortGroupAction->addAction(sortShutterSpeedAction);
    sortGroupAction->addAction(sortISOAction);
    sortGroupAction->addAction(sortModelAction);
    sortGroupAction->addAction(sortLensAction);
    sortGroupAction->addAction(sortFocalLengthAction);
    sortGroupAction->addAction(sortTitleAction);
    sortGroupAction->addAction(sortCreatorAction);

    updateSortColumn(sortColumn);

    sortReverseAction = new QAction(tr("Reverse sort order"), this);
    sortReverseAction->setObjectName("reverse");
    sortReverseAction->setShortcutVisibleInContextMenu(true);
    sortReverseAction->setCheckable(true);
    addAction(sortReverseAction);
    connect(sortReverseAction, &QAction::triggered, this, &MW::toggleSortDirectionClick);
}

void MW::createEmbellishActions()
{
    // Embellish menu
    int n;          // used to populate action lists

    embelExportAction = new QAction(tr("Export using ..."), this);
    embelExportAction->setObjectName("embelExportAct");
    embelExportAction->setShortcutVisibleInContextMenu(true);
    addAction(embelExportAction);

    embelNewTemplateAction = new QAction(tr("New template"), this);
    embelNewTemplateAction->setObjectName("newEmbelTemplateAct");
    embelNewTemplateAction->setShortcutVisibleInContextMenu(true);
    addAction(embelNewTemplateAction);
    connect(embelNewTemplateAction, &QAction::triggered, this, &MW::newEmbelTemplate);

    embelReadTemplateAction = new QAction(tr("Read template from file"), this);
    embelReadTemplateAction->setObjectName("embelExportAct");
    embelReadTemplateAction->setShortcutVisibleInContextMenu(true);
    addAction(embelReadTemplateAction);
    connect(embelReadTemplateAction, &QAction::triggered, embelProperties, &EmbelProperties::readTemplateFromFile);

    embelSaveTemplateAction = new QAction(tr("Save current template to file"), this);
    embelSaveTemplateAction->setObjectName("embelExportAct");
    embelSaveTemplateAction->setShortcutVisibleInContextMenu(true);
    addAction(embelSaveTemplateAction);
    connect(embelSaveTemplateAction, &QAction::triggered, embelProperties, &EmbelProperties::saveTemplateToFile);

    embelManageTilesAction = new QAction(tr("Manage tiles"), this);
    embelManageTilesAction->setObjectName("embelManageTilesAct");
    embelManageTilesAction->setShortcutVisibleInContextMenu(true);
    addAction(embelManageTilesAction);
    connect(embelManageTilesAction, &QAction::triggered, embelProperties, &EmbelProperties::manageTiles);

    embelManageGraphicsAction = new QAction(tr("Manage graphics"), this);
    embelManageGraphicsAction->setObjectName("embelManageGraphicsAct");
    embelManageGraphicsAction->setShortcutVisibleInContextMenu(true);
    addAction(embelManageGraphicsAction);
    connect(embelManageGraphicsAction, &QAction::triggered, embelProperties, &EmbelProperties::manageGraphics);

    QString revealText = "Reveal";
#ifdef Q_OS_WIN
    revealText = "Show Winnets in Explorer";
#endif
#ifdef Q_OS_MAC
    revealText = "Show Winnets in Finder";
#endif

    embelRevealWinnetsAction = new QAction(revealText, this);
    embelRevealWinnetsAction->setObjectName("RevealWinnetsAct");
    embelRevealWinnetsAction->setShortcutVisibleInContextMenu(true);
    addAction(embelRevealWinnetsAction);
    connect(embelRevealWinnetsAction, &QAction::triggered, this, &MW::revealWinnets);

    /* general connection to handle invoking new embellish templates
     MacOS will not allow runtime menu insertions.  Cludge workaround
     add 30 dummy menu items and then hide until use.

     embelTemplatesActions are executed by EmbelProperties::invokeFromAction which in turn
     triggers EmbelProperties::itemChange.

     ie to execute "Do not embellish" (always the first action)
     EmbelProperties::invokeFromAction(embelTemplatesActions.at(0))
    */
    embelExportGroupAction = new QActionGroup(this);
    embelExportGroupAction->setExclusive(true);
    n = embelProperties->templateList.count() - 1;
    for (int i = 0; i < 30; i++) {
        QString name;
        QString objName = "";
        if (i < n) {
            name = embelProperties->templateList.at(i+1);
            objName = "template" + QString::number(i+1);
        }
        else name = "Future Template" + QString::number(i);

        embelTemplatesActions.append(new QAction(name, this));

        if (i < n) {
            embelTemplatesActions.at(i)->setObjectName(objName);
            //            embelTemplatesActions.at(i)->setCheckable(true);
            embelTemplatesActions.at(i)->setText(name);
        }

        if (i < n) addAction(embelTemplatesActions.at(i));

        if (i >= n) embelTemplatesActions.at(i)->setVisible(false);
        //        embelTemplatesActions.at(i)->setShortcut(QKeySequence("Ctrl+Shift+" + QString::number(i)));
        embelExportGroupAction->addAction(embelTemplatesActions.at(i));
    }
    addActions(embelTemplatesActions);
    /*
    // sync menu with QSettings last active embel template
    if (embelProperties->templateId < n)
        embelTemplatesActions.at(embelProperties->templateId)->setChecked(true);
        */
}

void MW::createViewActions()
{
    // View menu
    slideShowAction = new QAction(tr("Slide Show"), this);
    slideShowAction->setObjectName("slideShow");
    slideShowAction->setShortcutVisibleInContextMenu(true);
    addAction(slideShowAction);
    connect(slideShowAction, &QAction::triggered, this, &MW::slideShow);

    fullScreenAction = new QAction(tr("Full Screen"), this);
    fullScreenAction->setObjectName("fullScreenAct");
    fullScreenAction->setShortcutVisibleInContextMenu(true);
    fullScreenAction->setCheckable(true);
    if (isSettings && settings->contains("isFullScreen")) fullScreenAction->setChecked(settings->value("isFullScreen").toBool());
    else fullScreenAction->setChecked(false);
    addAction(fullScreenAction);
    connect(fullScreenAction, &QAction::triggered, this, &MW::toggleFullScreen);

    escapeFullScreenAction = new QAction(tr("Escape Full Screen"), this);
    escapeFullScreenAction->setObjectName("escapeFullScreenAct");
    escapeFullScreenAction->setShortcutVisibleInContextMenu(true);
    addAction(escapeFullScreenAction);
    connect(escapeFullScreenAction, &QAction::triggered, this, &MW::escapeFullScreen);

    ratingBadgeVisibleAction = new QAction(tr("Show Rating Badge"), this);
    ratingBadgeVisibleAction->setObjectName("toggleRatingBadge");
    ratingBadgeVisibleAction->setShortcutVisibleInContextMenu(true);
    ratingBadgeVisibleAction->setCheckable(true);
    ratingBadgeVisibleAction->setChecked(false);
    // isRatingBadgeVisible = false by default, loadSettings updates value before createActions
    ratingBadgeVisibleAction->setChecked(isRatingBadgeVisible);
    addAction(ratingBadgeVisibleAction);
    connect(ratingBadgeVisibleAction, &QAction::triggered, this, &MW::setRatingBadgeVisibility);

    iconNumberVisibleAction = new QAction(tr("Show Icon Number"), this);
    iconNumberVisibleAction->setObjectName("toggleIconNumber");
    iconNumberVisibleAction->setShortcutVisibleInContextMenu(true);
    iconNumberVisibleAction->setCheckable(true);
    iconNumberVisibleAction->setChecked(false);
    // isIconNumberVisible = false by default, loadSettings updates value before createActions
    iconNumberVisibleAction->setChecked(isIconNumberVisible);
    addAction(iconNumberVisibleAction);
    connect(iconNumberVisibleAction, &QAction::triggered, this, &MW::setIconNumberVisibility);

    infoVisibleAction = new QAction(tr("Show info overlay"), this);
    infoVisibleAction->setObjectName("toggleInfo");
    infoVisibleAction->setShortcutVisibleInContextMenu(true);
    infoVisibleAction->setCheckable(true);
    addAction(infoVisibleAction);
    connect(infoVisibleAction, &QAction::triggered, this, &MW::setShootingInfoVisibility);

    infoSelectAction = new QAction(tr("Select or edit info overlay   "), this);
    infoSelectAction->setShortcutVisibleInContextMenu(true);
    infoSelectAction->setObjectName("selectInfo");
    if (isSettings && settings->contains("isImageInfoVisible")) infoVisibleAction->setChecked(settings->value("isImageInfoVisible").toBool());
    else infoVisibleAction->setChecked(false);
    addAction(infoSelectAction);
    connect(infoSelectAction, &QAction::triggered, this, &MW::tokenEditor);

    asLoupeAction = new QAction(tr("Loupe"), this);
    asLoupeAction->setShortcutVisibleInContextMenu(true);
    asLoupeAction->setCheckable(true);
    if (isSettings && settings->contains("isLoupeDisplay"))
        asLoupeAction->setChecked(settings->value("isLoupeDisplay").toBool()
                                  || settings->value("isCompareDisplay").toBool());
    //                                  || setting->value("isEmbelDisplay").toBool()
    else asLoupeAction->setChecked(false);
    addAction(asLoupeAction);
    connect(asLoupeAction, &QAction::triggered, this, &MW::loupeDisplay);

    asGridAction = new QAction(tr("Grid"), this);
    asGridAction->setShortcutVisibleInContextMenu(true);
    asGridAction->setCheckable(true);
    if (isSettings && settings->contains("isGridDisplay")) asGridAction->setChecked(settings->value("isGridDisplay").toBool());
    else asGridAction->setChecked(true);
    addAction(asGridAction);
    connect(asGridAction, &QAction::triggered, this, &MW::gridDisplay);

    asTableAction = new QAction(tr("Table"), this);
    asTableAction->setShortcutVisibleInContextMenu(true);
    asTableAction->setCheckable(true);
    if (isSettings && settings->contains("isTableDisplay")) asTableAction->setChecked(settings->value("isTableDisplay").toBool());
    else asTableAction->setChecked(false);
    addAction(asTableAction);
    connect(asTableAction, &QAction::triggered, this, &MW::tableDisplay);

    asCompareAction = new QAction(tr("Compare"), this);
    asCompareAction->setShortcutVisibleInContextMenu(true);
    asCompareAction->setCheckable(true);
    asCompareAction->setChecked(false); // never start with compare set true
    addAction(asCompareAction);
    connect(asCompareAction, &QAction::triggered, this, &MW::compareDisplay);

    centralGroupAction = new QActionGroup(this);
    centralGroupAction->setExclusive(true);
    centralGroupAction->addAction(asLoupeAction);
    centralGroupAction->addAction(asGridAction);
    centralGroupAction->addAction(asTableAction);
    centralGroupAction->addAction(asCompareAction);

    zoomToAction = new QAction(tr("Zoom"), this);
    zoomToAction->setObjectName("zoomDlg");
    zoomToAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomToAction);
    connect(zoomToAction, &QAction::triggered, this, &MW::toggleZoomDlg);

    zoomOutAction = new QAction(tr("Zoom Out"), this);
    zoomOutAction->setObjectName("zoomOut");
    zoomOutAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomOutAction);
    connect(zoomOutAction, &QAction::triggered, this, &MW::zoomOut);

    zoomInAction = new QAction(tr("Zoom In"), this);
    zoomInAction->setObjectName("zoomIn");
    zoomInAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomInAction);
    connect(zoomInAction, &QAction::triggered, this, &MW::zoomIn);

    zoomToggleAction = new QAction(tr("Zoom fit <-> 100%"), this);
    zoomToggleAction->setObjectName("resetZoom");
    zoomToggleAction->setShortcutVisibleInContextMenu(true);
    addAction(zoomToggleAction);
    connect(zoomToggleAction, &QAction::triggered, this, &MW::zoomToggle);

    thumbsEnlargeAction = new QAction(tr("Enlarge thumbs"), this);
    thumbsEnlargeAction->setObjectName("enlargeThumbs");
    thumbsEnlargeAction->setShortcutVisibleInContextMenu(true);
    addAction(thumbsEnlargeAction);
    connect(thumbsEnlargeAction, &QAction::triggered, this, &MW::thumbsEnlarge);

    thumbsShrinkAction = new QAction(tr("Shrink thumbs"), this);
    thumbsShrinkAction->setObjectName("shrinkThumbs");
    thumbsShrinkAction->setShortcutVisibleInContextMenu(true);
    addAction(thumbsShrinkAction);
    connect(thumbsShrinkAction, &QAction::triggered, this, &MW::thumbsShrink);
}

void MW::createWindowActions()
{
    // Windows menu
    /* menu bar
    menuBarVisibleAction = new QAction(tr("Menubar"), this);
    menuBarVisibleAction->setObjectName("toggleMenuBar");
    menuBarVisibleAction->setShortcutVisibleInContextMenu(true);
    menuBarVisibleAction->setCheckable(true);
    if (isSettings && settings->contains("isMenuBarVisible")) menuBarVisibleAction->setChecked(settings->value("isMenuBarVisible").toBool());
    else menuBarVisibleAction->setChecked(true);
    addAction(menuBarVisibleAction);
    connect(menuBarVisibleAction, &QAction::triggered, this, &MW::setMenuBarVisibility);
    //*/

    statusBarVisibleAction = new QAction(tr("Statusbar"), this);
    statusBarVisibleAction->setObjectName("toggleStatusBar");
    statusBarVisibleAction->setShortcutVisibleInContextMenu(true);
    statusBarVisibleAction->setCheckable(true);
    if (isSettings && settings->contains("isStatusBarVisible")) statusBarVisibleAction->setChecked(settings->value("isStatusBarVisible").toBool());
    else statusBarVisibleAction->setChecked(true);
    addAction(statusBarVisibleAction);
    connect(statusBarVisibleAction, &QAction::triggered, this, &MW::setStatusBarVisibility);

    folderDockVisibleAction = new QAction(tr("Folder"), this);
    folderDockVisibleAction->setObjectName("toggleFiless");
    folderDockVisibleAction->setShortcutVisibleInContextMenu(true);
    folderDockVisibleAction->setCheckable(true);
    if (isSettings && settings->contains("isFolderDockVisible")) folderDockVisibleAction->setChecked(settings->value("isFolderDockVisible").toBool());
    else folderDockVisibleAction->setChecked(true);
    addAction(folderDockVisibleAction);
    connect(folderDockVisibleAction, &QAction::triggered, this, &MW::toggleFolderDockVisibility);

    favDockVisibleAction = new QAction(tr("Bookmarks"), this);
    favDockVisibleAction->setObjectName("toggleFavs");
    favDockVisibleAction->setShortcutVisibleInContextMenu(true);
    favDockVisibleAction->setCheckable(true);
    if (isSettings && settings->contains("isFavDockVisible")) favDockVisibleAction->setChecked(settings->value("isFavDockVisible").toBool());
    else favDockVisibleAction->setChecked(true);
    addAction(favDockVisibleAction);
    connect(favDockVisibleAction, &QAction::triggered, this, &MW::toggleFavDockVisibility);

    filterDockVisibleAction = new QAction(tr("Filters"), this);
    filterDockVisibleAction->setObjectName("toggleFilters");
    filterDockVisibleAction->setShortcutVisibleInContextMenu(true);
    filterDockVisibleAction->setCheckable(true);
    if (isSettings && settings->contains("isFilterDockVisible")) filterDockVisibleAction->setChecked(settings->value("isFilterDockVisible").toBool());
    else filterDockVisibleAction->setChecked(true);
    addAction(filterDockVisibleAction);
    connect(filterDockVisibleAction, &QAction::triggered, this, &MW::toggleFilterDockVisibility);

    metadataDockVisibleAction = new QAction(tr("Metadata"), this);
    metadataDockVisibleAction->setObjectName("toggleMetadata");
    metadataDockVisibleAction->setShortcutVisibleInContextMenu(true);
    metadataDockVisibleAction->setCheckable(true);
    if (isSettings && settings->contains("isMetadataDockVisible")) metadataDockVisibleAction->setChecked(settings->value("isMetadataDockVisible").toBool());
    else metadataDockVisibleAction->setChecked(true);
    addAction(metadataDockVisibleAction);
    connect(metadataDockVisibleAction, &QAction::triggered, this, &MW::toggleMetadataDockVisibility);

    thumbDockVisibleAction = new QAction(tr("Thumbnails"), this);
    thumbDockVisibleAction->setObjectName("toggleThumbs");
    thumbDockVisibleAction->setShortcutVisibleInContextMenu(true);
    thumbDockVisibleAction->setCheckable(true);
    if (isSettings && settings->contains("isThumbDockVisible")) thumbDockVisibleAction->setChecked(settings->value("isThumbDockVisible").toBool());
    else thumbDockVisibleAction->setChecked(true);
    addAction(thumbDockVisibleAction);
    connect(thumbDockVisibleAction, &QAction::triggered, this, &MW::toggleThumbDockVisibity);

    embelDockVisibleAction = new QAction(tr("Embellish Editor"), this);
    embelDockVisibleAction->setObjectName("toggleEmbelDock");
    embelDockVisibleAction->setShortcutVisibleInContextMenu(true);
    embelDockVisibleAction->setCheckable(true);
    if (isSettings && settings->contains("isEmbelDockVisible")) embelDockVisibleAction->setChecked(settings->value("isEmbelDockVisible").toBool());
    else embelDockVisibleAction->setChecked(false);
    addAction(embelDockVisibleAction);
    connect(embelDockVisibleAction, &QAction::triggered, this, &MW::toggleEmbelDockVisibility);

    // rgh delete this ?
    metadataFixedSizeAction = new QAction(tr("Metadata Panel Fix Size"), this);
    metadataFixedSizeAction->setObjectName("metadataFixedSize");
    metadataFixedSizeAction->setShortcutVisibleInContextMenu(true);
    metadataFixedSizeAction->setCheckable(true);
    addAction(metadataFixedSizeAction);
    connect(metadataFixedSizeAction, &QAction::triggered, this, &MW::setMetadataDockFixedSize);

    // Workspace submenu of Window menu
    defaultWorkspaceAction = new QAction(tr("Default Workspace"), this);
    defaultWorkspaceAction->setObjectName("defaultWorkspace");
    defaultWorkspaceAction->setShortcutVisibleInContextMenu(true);
    addAction(defaultWorkspaceAction);
    connect(defaultWorkspaceAction, &QAction::triggered, this, &MW::defaultWorkspace);

    newWorkspaceAction = new QAction(tr("New Workspace"), this);
    newWorkspaceAction->setObjectName("newWorkspace");
    newWorkspaceAction->setShortcutVisibleInContextMenu(true);
    addAction(newWorkspaceAction);
    connect(newWorkspaceAction, &QAction::triggered, this, &MW::newWorkspace);

    manageWorkspaceAction = new QAction(tr("Manage Workspaces ..."), this);
    manageWorkspaceAction->setObjectName("manageWorkspaces");
    manageWorkspaceAction->setShortcutVisibleInContextMenu(true);
    addAction(manageWorkspaceAction);
    connect(manageWorkspaceAction, &QAction::triggered, this, &MW::manageWorkspaces);

    // general connection to handle invoking new workspaces
    // MacOS will not allow runtime menu insertions.  Cludge workaround
    // add 10 dummy menu items and then hide until use.
    int n = workspaces->count();
    for (int i = 0; i < 10; i++) {
        QString name;
        QString objName = "";
        if (i < n) {
            name = workspaces->at(i).name;
            objName = "workspace" + QString::number(i);
        }
        else name = "Future Workspace" + QString::number(i);

        workspaceActions.append(new QAction(name, this));
        if (i < n) {
            workspaceActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
            workspaceActions.at(i)->setObjectName(objName);
            workspaceActions.at(i)->setShortcutVisibleInContextMenu(true);
            workspaceActions.at(i)->setText(name);
            workspaceActions.at(i)->setVisible(true);
            addAction(workspaceActions.at(i));
        }
        if (i >= n) workspaceActions.at(i)->setVisible(false);
        workspaceActions.at(i)->setShortcut(QKeySequence("Ctrl+" + QString::number(i)));
    }
    addActions(workspaceActions);
}

void MW::createHelpActions()
{
    //Help menu

    checkForUpdateAction = new QAction(tr("Check for updates"), this);
    checkForUpdateAction->setObjectName("CheckForUpdate");
    checkForUpdateAction->setShortcutVisibleInContextMenu(true);
    addAction(checkForUpdateAction);
    connect(checkForUpdateAction, &QAction::triggered, this, &MW::checkForUpdate);

    aboutAction = new QAction(tr("About"), this);
    aboutAction->setObjectName("about");
    aboutAction->setShortcutVisibleInContextMenu(true);
    addAction(aboutAction);
    connect(aboutAction, &QAction::triggered, this, &MW::about);

    helpAction = new QAction(tr("Winnow Help"), this);
    helpAction->setObjectName("help");
    helpAction->setShortcutVisibleInContextMenu(true);
    addAction(helpAction);
    connect(helpAction, &QAction::triggered, this, &MW::help);

    helpShortcutsAction = new QAction(tr("Winnow Shortcuts"), this);
    helpShortcutsAction->setObjectName("helpShortcuts");
    helpShortcutsAction->setShortcutVisibleInContextMenu(true);
    addAction(helpShortcutsAction);
    connect(helpShortcutsAction, &QAction::triggered, this, &MW::helpShortcuts);

    helpWelcomeAction = new QAction(tr("Show welcome screen"), this);
    helpWelcomeAction->setObjectName("helpWelcome");
    helpWelcomeAction->setShortcutVisibleInContextMenu(true);
    addAction(helpWelcomeAction);
    connect(helpWelcomeAction, &QAction::triggered, this, &MW::helpWelcome);

    helpRevealLogFileAction = new QAction("Send log file to Rory", this);
    helpRevealLogFileAction->setObjectName("RevealLogFileAct");
    helpRevealLogFileAction->setShortcutVisibleInContextMenu(true);
    addAction(helpRevealLogFileAction);
    connect(helpRevealLogFileAction, &QAction::triggered, this, &MW::revealLogFile);

    // Help Diagnostics

    diagnosticsAllAction = new QAction(tr("All Diagnostics"), this);
    diagnosticsAllAction->setObjectName("diagnosticsAll");
    diagnosticsAllAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsAllAction);
    connect(diagnosticsAllAction, &QAction::triggered, this, &MW::diagnosticsAll);

    diagnosticsCurrentAction = new QAction(tr("Current Row Diagnostics"), this);
    diagnosticsCurrentAction->setObjectName("diagnosticsCurrent");
    diagnosticsCurrentAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsCurrentAction);
    connect(diagnosticsCurrentAction, &QAction::triggered, this, &MW::diagnosticsCurrent);

    diagnosticsErrorsAction = new QAction(tr("Error log"), this);
    diagnosticsErrorsAction->setObjectName("diagnosticsErrorsAction");
    diagnosticsErrorsAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsErrorsAction);
    connect(diagnosticsErrorsAction, &QAction::triggered, this, &MW::errorReport);

    diagnosticsLogAction = new QAction(tr("Activity log"), this);
    diagnosticsLogAction->setObjectName("diagnosticsLogAction");
    diagnosticsLogAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsLogAction);
    connect(diagnosticsLogAction, &QAction::triggered, this, &MW::logReport);

    diagnosticsMainAction = new QAction(tr("Main diagnostics"), this);
    diagnosticsMainAction->setObjectName("diagnosticsMain");
    diagnosticsMainAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsMainAction);
    connect(diagnosticsMainAction, &QAction::triggered, this, &MW::diagnosticsMain);

    diagnosticsGridViewAction = new QAction(tr("GridView diagnostics"), this);
    diagnosticsGridViewAction->setObjectName("diagnosticsGridView");
    diagnosticsGridViewAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsGridViewAction);
    connect(diagnosticsGridViewAction, &QAction::triggered, this, &MW::diagnosticsGridView);

    diagnosticsThumbViewAction = new QAction(tr("ThumbView diagnostics"), this);
    diagnosticsThumbViewAction->setObjectName("diagnosticsThumbView");
    diagnosticsThumbViewAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsThumbViewAction);
    connect(diagnosticsThumbViewAction, &QAction::triggered, this, &MW::diagnosticsThumbView);

    diagnosticsImageViewAction = new QAction(tr("ImageView diagnostics"), this);
    diagnosticsImageViewAction->setObjectName("diagnosticsImageView");
    diagnosticsImageViewAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsImageViewAction);
    connect(diagnosticsImageViewAction, &QAction::triggered, this, &MW::diagnosticsImageView);

    diagnosticsMetadataAction = new QAction(tr("Metadata diagnostics"), this);
    diagnosticsMetadataAction->setObjectName("diagnosticsMetadata");
    diagnosticsMetadataAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsMetadataAction);
    connect(diagnosticsMetadataAction, &QAction::triggered, this, &MW::diagnosticsMetadata);

    diagnosticsDataModelAction = new QAction(tr("DataModel diagnostics"), this);
    diagnosticsDataModelAction->setObjectName("diagnosticsDataModel");
    diagnosticsDataModelAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsDataModelAction);
    connect(diagnosticsDataModelAction, &QAction::triggered, this, &MW::diagnosticsDataModel);

    diagnosticsMetadataCacheAction = new QAction(tr("MetadataCache diagnostics"), this);
    diagnosticsMetadataCacheAction->setObjectName("diagnosticsMetadataCache");
    diagnosticsMetadataCacheAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsMetadataCacheAction);
    connect(diagnosticsMetadataCacheAction, &QAction::triggered, this, &MW::diagnosticsMetadataCache);

    diagnosticsImageCacheAction = new QAction(tr("ImageCache diagnostics"), this);
    diagnosticsImageCacheAction->setObjectName("diagnosticsImageCache");
    diagnosticsImageCacheAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsImageCacheAction);
    connect(diagnosticsImageCacheAction, &QAction::triggered, this, &MW::diagnosticsImageCache);

    diagnosticsEmbellishAction = new QAction(tr("Embellish diagnostics"), this);
    diagnosticsEmbellishAction->setObjectName("diagnosticsEmbellish");
    diagnosticsEmbellishAction->setShortcutVisibleInContextMenu(true);
    addAction(diagnosticsEmbellishAction);
    connect(diagnosticsEmbellishAction, &QAction::triggered, this, &MW::diagnosticsEmbellish);

    // Tests
    traverseFolderStressTestAction = new QAction(tr("Traverse images in current folder stress test"), this);
    traverseFolderStressTestAction->setObjectName("traversFoldersStressTest");
    traverseFolderStressTestAction->setShortcutVisibleInContextMenu(true);
    addAction(traverseFolderStressTestAction);
    connect(traverseFolderStressTestAction, &QAction::triggered, this, &MW::traverseFolderStressTestFromMenu);

    // Tests
    bounceFoldersStressTestAction = new QAction(tr("Bounce bookmarks stress test"), this);
    bounceFoldersStressTestAction->setObjectName("bounceBookmarksStressTest");
    bounceFoldersStressTestAction->setShortcutVisibleInContextMenu(true);
    addAction(bounceFoldersStressTestAction);
    connect(bounceFoldersStressTestAction, &QAction::triggered, this, &MW::bounceFoldersStressTestFromMenu);
}

void MW::createMiscActions()
{
    copyInfoTextToClipboardAction = new QAction(tr("Copy info field text"), this);
    copyInfoTextToClipboardAction->setObjectName("copyInfoviewText");
    copyInfoTextToClipboardAction->setShortcutVisibleInContextMenu(true);
    addAction(copyInfoTextToClipboardAction);
    connect(copyInfoTextToClipboardAction, &QAction::triggered, infoView, &InfoView::copyEntry);

    // Non- Menu actions
    thriftyCacheAction = new QAction(tr("Thrifty Cache"), this);
    addAction(thriftyCacheAction);
    thriftyCacheAction->setShortcut(QKeySequence("F10"));
    connect(thriftyCacheAction, &QAction::triggered, this, &MW::thriftyCache);

    moderateCacheAction = new QAction(tr("Moderate Cache"), this);
    addAction(moderateCacheAction);
    moderateCacheAction->setShortcut(QKeySequence("F11"));
    connect(moderateCacheAction, &QAction::triggered, this, &MW::moderateCache);

    greedyCacheAction = new QAction(tr("Greedy Cache"), this);
    addAction(greedyCacheAction);
    greedyCacheAction->setShortcut(QKeySequence("F12"));
    connect(greedyCacheAction, &QAction::triggered, this, &MW::greedyCache);

    // Rory (extra functionality)
    roryAction = new QAction(tr("Test"), this);
    roryAction->setObjectName("rory");
    roryAction->setShortcutVisibleInContextMenu(true);
    addAction(roryAction);
    roryAction->setShortcut(QKeySequence("Shift+Ctrl+Alt+."));
    connect(roryAction, &QAction::triggered, this, &MW::toggleRory);

    // Testing

    testAction = new QAction(tr("Test"), this);
    testAction->setObjectName("test");
    testAction->setShortcutVisibleInContextMenu(true);
    addAction(testAction);
    testAction->setShortcut(QKeySequence("Shift+Ctrl+Alt+T"));
    connect(testAction, &QAction::triggered, this, &MW::test);

    testAction1 = new QAction(tr("Test1"), this);
    testAction1->setObjectName("test1");
    testAction1->setShortcutVisibleInContextMenu(true);
    addAction(testAction1);
    testAction1->setShortcut(QKeySequence("/"));
    connect(testAction1, &QAction::triggered, this, &MW::test);

    testNewFileFormatAction = new QAction(tr("Test Metadata"), this);
    testNewFileFormatAction->setObjectName("testNewFileFormat");
    testNewFileFormatAction->setShortcutVisibleInContextMenu(true);
    addAction(testNewFileFormatAction);
    testNewFileFormatAction->setShortcut(QKeySequence("Shift+Ctrl+Alt+F"));
    connect(testNewFileFormatAction, &QAction::triggered, this, &MW::testNewFileFormat);

    // Possibly needed actions

    //    enterAction = new QAction(tr("Enter"), this);
    //    enterAction->setObjectName("enterAction");
    //    enterAction->setShortcut(QKeySequence("X"));
    //    connect(enterAction, SIGNAL(triggered()), this, SLOT(enter()));

    // used in fsTree and bookmarks
    pasteFilesAction = new QAction(tr("Paste files"), this);
    pasteFilesAction->setObjectName("paste");
    pasteFilesAction->setShortcutVisibleInContextMenu(true);
    pasteFilesAction->setShortcut(QKeySequence("Ctrl+V"));
    addAction(pasteFilesAction);
    connect(pasteFilesAction, &QAction::triggered, this, &MW::pasteFiles);

    // initially enable/disable actions
    embedThumbnailsAction->setEnabled(G::modifySourceFiles);
}

void MW::createMenus()
{
    if (G::isLogger) G::log("MW::createMenus");

    createFileMenu();
    createEditMenu();
    createGoMenu();
    createFilterMenu();
    createSortMenu();
    createEmbellishMenu();
    createViewMenu();
    createWindowMenu();
    createHelpMenu();

    createMainMenu();
    createMainContextMenu();
    createFSTreeContextMenu();
    createBookmarksContextMenu();
    createFiltersContextMenu();
    createInfoViewContextMenu();
    createThumbViewContextMenu();

    QLabel *label = new QLabel;
    label->setText(" TEST ");
    label->setStyleSheet("QLabel{color:yellow;}");
    QToolBar *toolBar = new QToolBar;
    toolBar->addWidget(label);

    enableSelectionDependentMenus();
}

void MW::createFileMenu()
{
    fileMenu = new QMenu(this);
    fileMenu->setToolTipsVisible(true);
    fileGroupAct = new QAction("File", this);
    fileGroupAct->setMenu(fileMenu);
    fileMenu->addAction(openAction);
    fileMenu->addAction(openUsbAction);
    openWithMenu = fileMenu->addMenu(tr("Open with..."));
    openWithMenu->addAction(manageAppAction);
    openWithMenu->addSeparator();
    // add 10 dummy menu items for external apps
    for (int i = 0; i < 10; i++) {
        openWithMenu->addAction(appActions.at(i));
    }
    recentFoldersMenu = fileMenu->addMenu(tr("Recent folders"));
    // add maxRecentFolders dummy menu items for custom workspaces
    for (int i = 0; i < maxRecentFolders; i++) {
        recentFoldersMenu->addAction(recentFolderActions.at(i));
    }
    connect(recentFoldersMenu, SIGNAL(triggered(QAction*)),
            SLOT(invokeRecentFolder(QAction*)));
    fileMenu->addAction(revealFileAction);

    fileMenu->addSeparator();
    fileMenu->addAction(refreshCurrentAction);
    fileMenu->addAction(refreshFoldersAction);

    fileMenu->addSeparator();
    fileMenu->addAction(ingestAction);
    ingestHistoryFoldersMenu = fileMenu->addMenu(tr("Ingest History"));
    // add maxIngestHistoryFolders dummy menu items for custom workspaces
    for (int i = 0; i < maxIngestHistoryFolders; i++) {
        ingestHistoryFoldersMenu->addAction(ingestHistoryFolderActions.at(i));
    }
    connect(ingestHistoryFoldersMenu, SIGNAL(triggered(QAction*)),
            SLOT(invokeIngestHistoryFolder(QAction*)));
    fileMenu->addAction(ejectAction);
    fileMenu->addAction(eraseUsbAction);

    fileMenu->addSeparator();
    fileMenu->addAction(colorManageAction);
    fileMenu->addAction(combineRawJpgAction);
    fileMenu->addAction(subFoldersAction);
    fileMenu->addAction(addBookmarkAction);
    fileMenu->addSeparator();
    fileMenu->addAction(renameAction);
    fileMenu->addAction(saveAsFileAction);
    fileMenu->addSeparator();
    fileMenu->addAction(reportMetadataAction);
    //    fileMenu->addAction(mediaReadSpeedAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);    // Appears in Winnow menu in OSX
}

void MW::createEditMenu()
{
    editMenu = new QMenu(this);
    editGroupAct = new QAction("Edit", this);
    editGroupAct->setMenu(editMenu);
    editMenu->addAction(selectAllAction);
    editMenu->addAction(invertSelectionAction);
    editMenu->addSeparator();
#ifdef Q_OS_MAC
    editMenu->addAction(shareFilesAction);
#endif
    editMenu->addAction(copyFilesAction);
    editMenu->addAction(copyImageAction);
    editMenu->addAction(copyImagePathFromContextAction);
    editMenu->addAction(pasteFilesAction);
    editMenu->addSeparator();
    editMenu->addAction(deleteImagesAction);
    editMenu->addAction(deleteActiveFolderAction);
    editMenu->addSeparator();
    editMenu->addAction(pickAction);
    editMenu->addAction(rejectAction);
    editMenu->addAction(pickUnlessRejectedAction);
    //    editMenu->addAction(filterPickAction);
    editMenu->addAction(popPickHistoryAction);
    editMenu->addSeparator();
    editMenu->addAction(searchTextEditAction);
    editMenu->addSeparator();
    ratingsMenu = editMenu->addMenu("Ratings");
    ratingsMenu->addAction(rate0Action);
    ratingsMenu->addAction(rate1Action);
    ratingsMenu->addAction(rate2Action);
    ratingsMenu->addAction(rate3Action);
    ratingsMenu->addAction(rate4Action);
    ratingsMenu->addAction(rate5Action);
    //    editMenu->addSeparator();
    labelsMenu = editMenu->addMenu("Color labels");
    labelsMenu->addAction(label0Action);
    labelsMenu->addAction(label1Action);
    labelsMenu->addAction(label2Action);
    labelsMenu->addAction(label3Action);
    labelsMenu->addAction(label4Action);
    labelsMenu->addAction(label5Action);
    editMenu->addSeparator();
    editMenu->addAction(rotateRightAction);
    editMenu->addAction(rotateLeftAction);
    editMenu->addSeparator();
    utilitiesMenu = editMenu->addMenu("Utilities");
    utilitiesMenu->addAction(mediaReadSpeedAction);
    utilitiesMenu->addAction(visCmpImagesAction);
    utilitiesMenu->addAction(embedThumbnailsAction);
    utilitiesMenu->addAction(reportHueCountAction);
    utilitiesMenu->addAction(meanStackAction);
    editMenu->addSeparator();
    editMenu->addAction(prefAction);       // Appears in Winnow menu in OSX
}

void MW::createGoMenu()
{
    goMenu = new QMenu(this);
    goGroupAct = new QAction("Go", this);
    goGroupAct->setMenu(goMenu);
    /* moved to MW::eveeentFilter
    goMenu->addAction(keyRightAction);
    goMenu->addAction(keyLeftAction);
    goMenu->addAction(keyUpAction);
    goMenu->addAction(keyDownAction);
    goMenu->addAction(keyPageUpAction);
    goMenu->addAction(keyPageDownAction);
    goMenu->addAction(keyHomeAction);
    goMenu->addAction(keyEndAction);
    goMenu->addSeparator();
    */
    goMenu->addAction(jumpAction);
    goMenu->addSeparator();
    goMenu->addAction(keyScrollLeftAction);
    goMenu->addAction(keyScrollRightAction);
    goMenu->addAction(keyScrollUpAction);
    goMenu->addAction(keyScrollDownAction);
    goMenu->addAction(keyScrollPageLeftAction);
    goMenu->addAction(keyScrollPageRightAction);
    goMenu->addAction(keyScrollPageUpAction);
    goMenu->addAction(keyScrollPageDownAction);
    goMenu->addSeparator();
    goMenu->addAction(nextPickAction);
    goMenu->addAction(prevPickAction);
    goMenu->addSeparator();
    goMenu->addAction(randomImageAction);
}

void MW::createFilterMenu()
{
    filterMenu = new QMenu(this);
    filterGroupAct = new QAction("Filter", this);
    filterGroupAct->setMenu(filterMenu);
    filterMenu->addAction(filterUpdateAction);
    filterMenu->addAction(filterInvertAction);
    filterMenu->addAction(clearAllFiltersAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterPickAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterSearchAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterRating1Action);
    filterMenu->addAction(filterRating2Action);
    filterMenu->addAction(filterRating3Action);
    filterMenu->addAction(filterRating4Action);
    filterMenu->addAction(filterRating5Action);
    filterMenu->addSeparator();
    filterMenu->addAction(filterRedAction);
    filterMenu->addAction(filterYellowAction);
    filterMenu->addAction(filterGreenAction);
    filterMenu->addAction(filterBlueAction);
    filterMenu->addAction(filterPurpleAction);
    filterMenu->addSeparator();
    filterMenu->addAction(filterLastDayAction);
    filterMenu->addSeparator();
    //    filterMenu->addAction(filterUpdateAction);
    //    filterMenu->addAction(filterInvertAction);
    filterMenu->addAction(filterLastDayAction);
}

void MW::createSortMenu()
{
    sortMenu = new QMenu(this);
    sortGroupAct = new QAction("Sort", this);
    sortGroupAct->setMenu(sortMenu);
    sortMenu->addActions(sortGroupAction->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(sortReverseAction);
}

void MW::createEmbellishMenu()
{
    embelMenu = new QMenu(this);
    //    /*
    embelMenu->setIcon(QIcon(":/images/icon16/lightning.png"));
    //*/
    embelGroupAct = new QAction("Embellish", this);
    embelGroupAct->setMenu(embelMenu);
    embelExportMenu = embelMenu->addMenu(tr("Export embellished images..."));
    embelExportMenu->addActions(embelExportGroupAction->actions());
    embelMenu->addSeparator();
    embelMenu->addAction(embelNewTemplateAction);
    embelMenu->addAction(embelReadTemplateAction);
    embelMenu->addAction(embelSaveTemplateAction);
    embelMenu->addSeparator();
    embelMenu->addAction(embelManageTilesAction);
    embelMenu->addAction(embelManageGraphicsAction);
    embelMenu->addSeparator();
    embelMenu->addAction(embelRevealWinnetsAction);
    connect(embelExportMenu, &QMenu::triggered, this, &MW::exportEmbelFromAction);
    //connect(embelMenu, &QMenu::triggered, embelProperties, &EmbelProperties::invokeFromAction);
}

void MW::createViewMenu()
{
    viewMenu = new QMenu(this);
    viewGroupAct = new QAction("View", this);
    viewGroupAct->setMenu(viewMenu);
    viewMenu->addActions(centralGroupAction->actions());
    viewMenu->addSeparator();
    viewMenu->addAction(slideShowAction);
    viewMenu->addAction(fullScreenAction);
    viewMenu->addAction(escapeFullScreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(ratingBadgeVisibleAction);
    viewMenu->addAction(iconNumberVisibleAction);
    viewMenu->addAction(infoVisibleAction);
    viewMenu->addAction(infoSelectAction);
    viewMenu->addSeparator();
    viewMenu->addAction(zoomToAction);
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(zoomToggleAction);
    viewMenu->addSeparator();
    viewMenu->addAction(thumbsEnlargeAction);
    viewMenu->addAction(thumbsShrinkAction);

}

void MW::createWindowMenu()
{
    windowMenu = new QMenu(this);
    windowGroupAct = new QAction("Window", this);
    windowGroupAct->setMenu(windowMenu);
    workspaceMenu = windowMenu->addMenu(tr("&Workspace"));
    workspaceMenu->addAction(defaultWorkspaceAction);
    workspaceMenu->addAction(newWorkspaceAction);
    workspaceMenu->addAction(manageWorkspaceAction);
    workspaceMenu->addSeparator();
    // add 10 dummy menu items for custom workspaces
    for (int i=0; i<10; i++) {
        workspaceMenu->addAction(workspaceActions.at(i));
    }
    connect(workspaceMenu, SIGNAL(triggered(QAction*)),
            SLOT(invokeWorkspaceFromAction(QAction*)));
    windowMenu->addSeparator();
    windowMenu->addAction(folderDockVisibleAction);
    windowMenu->addAction(favDockVisibleAction);
    windowMenu->addAction(filterDockVisibleAction);
    windowMenu->addAction(metadataDockVisibleAction);
    windowMenu->addAction(thumbDockVisibleAction);
    if (!hideEmbellish) windowMenu->addAction(embelDockVisibleAction);
    windowMenu->addSeparator();
//    windowMenu->addAction(windowTitleBarVisibleAction);
    #ifdef Q_OS_WIN
    //windowMenu->addAction(menuBarVisibleAction);
    #endif
    windowMenu->addAction(statusBarVisibleAction);  // crash
}

void MW::createHelpMenu()
{
    helpMenu = new QMenu(this);
    helpGroupAct = new QAction("Help", this);
    helpGroupAct->setMenu(helpMenu);
    #ifdef Q_OS_WIN
    helpMenu->addAction(checkForUpdateAction);
    helpMenu->addSeparator();
    #endif
    helpMenu->addAction(aboutAction);
    //    helpMenu->addAction(helpAction);
    helpMenu->addAction(helpShortcutsAction);
    helpMenu->addAction(helpWelcomeAction);
    helpMenu->addSeparator();
    helpMenu->addAction(diagnosticsErrorsAction);
    helpMenu->addAction(diagnosticsLogAction);
    //    helpMenu->addSeparator();
    //    helpMenu->addAction(helpRevealLogFileAction);
    helpMenu->addSeparator();
    helpDiagnosticsMenu = helpMenu->addMenu(tr("&Diagnostics"));
    testMenu = helpDiagnosticsMenu->addMenu(tr("&Tests"));
    testMenu->addAction(traverseFolderStressTestAction);
    testMenu->addAction(bounceFoldersStressTestAction);
    helpDiagnosticsMenu->addAction(diagnosticsAllAction);
    helpDiagnosticsMenu->addAction(diagnosticsCurrentAction);
    helpDiagnosticsMenu->addAction(diagnosticsMainAction);
    helpDiagnosticsMenu->addAction(diagnosticsGridViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsThumbViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsImageViewAction);
    helpDiagnosticsMenu->addAction(diagnosticsMetadataAction);
    helpDiagnosticsMenu->addAction(diagnosticsDataModelAction);
    helpDiagnosticsMenu->addAction(diagnosticsMetadataCacheAction);
    helpDiagnosticsMenu->addAction(diagnosticsImageCacheAction);
    helpDiagnosticsMenu->addAction(diagnosticsEmbellishAction);
}

void MW::createMainMenu()
{
    // MAIN MENU
    menuBar()->addAction(fileGroupAct);
    menuBar()->addAction(editGroupAct);
    menuBar()->addMenu(goMenu);
    menuBar()->addAction(goGroupAct);
    menuBar()->addAction(filterGroupAct);
    menuBar()->addAction(sortGroupAct);
    menuBar()->addAction(embelGroupAct);
    menuBar()->addAction(viewGroupAct);
    menuBar()->addAction(windowGroupAct);
    menuBar()->addAction(helpGroupAct);
    menuBar()->setVisible(true);
}

void MW::createMainContextMenu()
{
    // MAIN CONTEXT MENU
    mainContextActions = new QList<QAction *>;
    mainContextActions->append(fileGroupAct);
    mainContextActions->append(editGroupAct);
    mainContextActions->append(goGroupAct);
    mainContextActions->append(filterGroupAct);
    mainContextActions->append(sortGroupAct);
    mainContextActions->append(embelGroupAct);
    mainContextActions->append(viewGroupAct);
    mainContextActions->append(windowGroupAct);
    mainContextActions->append(helpGroupAct);
    // Central Widget mode context menu
    centralWidget->addActions(*mainContextActions);
    centralWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void MW::createFSTreeContextMenu()
{
    // FSTREE CONTEXT MENU
    // Separator Action
    QAction *separatorAction = new QAction(this);
    separatorAction->setSeparator(true);
    QAction *separatorAction1 = new QAction(this);
    separatorAction1->setSeparator(true);
    QAction *separatorAction2 = new QAction(this);
    separatorAction2->setSeparator(true);
    QAction *separatorAction3 = new QAction(this);
    separatorAction3->setSeparator(true);
    fsTreeActions = new QList<QAction *>;
    //    QList<QAction *> *fsTreeActions = new QList<QAction *>;
    fsTreeActions->append(refreshCurrentAction);
    fsTreeActions->append(refreshFoldersAction);
    fsTreeActions->append(collapseFoldersAction);
    fsTreeActions->append(separatorAction);
    fsTreeActions->append(ejectActionFromContextMenu);
    fsTreeActions->append(eraseUsbActionFromContextMenu);
    fsTreeActions->append(separatorAction1);
    //    fsTreeActions->append(showImageCountAction);
    fsTreeActions->append(revealFileActionFromContext);
    fsTreeActions->append(copyFolderPathFromContextAction);
    fsTreeActions->append(deleteFSTreeFolderAction);
    fsTreeActions->append(separatorAction2);
    fsTreeActions->append(pasteFilesAction);
    fsTreeActions->append(separatorAction3);
    fsTreeActions->append(addBookmarkActionFromContext);
    // docking panels context menus
    fsTree->addActions(*fsTreeActions);
    fsTree->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void MW::createBookmarksContextMenu()
{
    // Separator Action
    QAction *separatorAction = new QAction(this);
    separatorAction->setSeparator(true);
    QAction *separatorAction1 = new QAction(this);
    separatorAction1->setSeparator(true);
    QAction *separatorAction2 = new QAction(this);
    separatorAction2->setSeparator(true);
    QList<QAction *> *favActions = new QList<QAction *>;
    favActions->append(refreshBookmarkAction);
    favActions->append(revealFileActionFromContext);
    favActions->append(copyFolderPathFromContextAction);
    favActions->append(separatorAction);
    favActions->append(ejectActionFromContextMenu);
    favActions->append(eraseUsbActionFromContextMenu);
    favActions->append(separatorAction1);
    favActions->append(pasteFilesAction);
    favActions->append(separatorAction2);
    favActions->append(removeBookmarkAction);

    // docking panels context menus
    bookmarks->addActions(*favActions);
    bookmarks->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void MW::createFiltersContextMenu()
{
    // Separator Action
    QAction *separatorAction = new QAction(this);
    separatorAction->setSeparator(true);
    QAction *separatorAction1 = new QAction(this);
    separatorAction1->setSeparator(true);
    QAction *separatorAction2 = new QAction(this);
    separatorAction2->setSeparator(true);
    filterActions = new QList<QAction *>;
    //    QList<QAction *> *filterActions = new QList<QAction *>;
    filterActions->append(filterUpdateAction);
    filterActions->append(clearAllFiltersAction);
    filterActions->append(filterInvertAction);
    filterActions->append(searchTextEditAction);
    filterActions->append(separatorAction);
    filterActions->append(expandAllFiltersAction);
    filterActions->append(collapseAllFiltersAction);
    filterActions->append(filterSoloAction);
    // docking panels context menus
    filters->addActions(*filterActions);
    filters->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void MW::createInfoViewContextMenu()
{
    // Separator Action
    QAction *separatorAction = new QAction(this);
    separatorAction->setSeparator(true);
    QAction *separatorAction1 = new QAction(this);
    separatorAction1->setSeparator(true);
    QAction *separatorAction2 = new QAction(this);
    separatorAction2->setSeparator(true);
    QList<QAction *> *metadataActions = new QList<QAction *>;
    //    metadataActions->append(infoView->copyInfoAction);
    metadataActions->append(copyInfoTextToClipboardAction);
    metadataActions->append(separatorAction);
    metadataActions->append(reportMetadataAction);
    metadataActions->append(prefInfoAction);
    //    metadataActions->append(separatorAction1);
    //    metadataActions->append(metadataDockLockAction);
    //    metadataActions->append(metadataFixedSizeAction);
    // docking panels context menus
    if (G::useInfoView) {
        infoView->addActions(*metadataActions);
        infoView->setContextMenuPolicy(Qt::ActionsContextMenu);
    }
}

void MW::createThumbViewContextMenu()
{
    // Separator Action
    QAction *separatorAction = new QAction(this);
    separatorAction->setSeparator(true);
    QAction *separatorAction1 = new QAction(this);
    separatorAction1->setSeparator(true);
    QAction *separatorAction2 = new QAction(this);
    separatorAction2->setSeparator(true);
    QAction *separatorAction3 = new QAction(this);
    separatorAction3->setSeparator(true);
    QAction *separatorAction4 = new QAction(this);
    separatorAction4->setSeparator(true);
    QAction *separatorAction5 = new QAction(this);
    separatorAction5->setSeparator(true);
    QAction *separatorAction6 = new QAction(this);
    separatorAction6->setSeparator(true);
    QAction *separatorAction7 = new QAction(this);
    separatorAction7->setSeparator(true);
    QAction *separatorAction8 = new QAction(this);
    separatorAction8->setSeparator(true);
    QAction *separatorAction9 = new QAction(this);
    separatorAction9->setSeparator(true);

    QAction *openWithGroupAct = new QAction(tr("Open with..."), this);
    openWithGroupAct->setMenu(openWithMenu);
    QAction *embelExportGroupAct = new QAction(tr("Embellish export..."), this);
    embelExportGroupAct->setMenu(embelExportMenu);

    QList<QAction *> *thumbViewActions = new QList<QAction *>;
    //    thumbViewActions->append(pickMouseOverAction);
    thumbViewActions->append(pickAction);
    thumbViewActions->append(rejectAction);
    thumbViewActions->append(pickUnlessRejectedAction);
    thumbViewActions->append(separatorAction8);
    thumbViewActions->append(revealFileAction);
    thumbViewActions->append(openWithGroupAct);
    thumbViewActions->append(embelExportGroupAct);
    thumbViewActions->append(separatorAction9);
    thumbViewActions->append(ratingBadgeVisibleAction);
    thumbViewActions->append(iconNumberVisibleAction);
    thumbViewActions->append(separatorAction);
    thumbViewActions->append(selectAllAction);
    thumbViewActions->append(invertSelectionAction);
    thumbViewActions->append(separatorAction1);
    thumbViewActions->append(rotateRightAction);
    thumbViewActions->append(rotateLeftAction);
    thumbViewActions->append(separatorAction2);
    //    thumbViewActions->append(thumbsWrapAction);
    thumbViewActions->append(thumbsEnlargeAction);
    thumbViewActions->append(thumbsShrinkAction);
    thumbViewActions->append(separatorAction3);
    thumbViewActions->append(sortReverseAction);
    thumbViewActions->append(separatorAction4);
    #ifdef Q_OS_MAC
    thumbViewActions->append(shareFilesAction);
    #endif
    thumbViewActions->append(copyFilesAction);
    thumbViewActions->append(copyImageAction);
    thumbViewActions->append(copyImagePathFromContextAction);
    thumbViewActions->append(saveAsFileAction);
    thumbViewActions->append(separatorAction5);
    thumbViewActions->append(renameAction);
    thumbViewActions->append(deleteImagesAction);
    thumbViewActions->append(separatorAction6);
    thumbViewActions->append(embedThumbnailsAction);
    thumbViewActions->append(separatorAction7);
    thumbViewActions->append(reportMetadataAction);
    thumbViewActions->append(diagnosticsCurrentAction);
    thumbViewActions->append(diagnosticsMetadataCacheAction);
    thumbViewActions->append(diagnosticsImageCacheAction);
    // docking panels context menus
    thumbView->addActions(*thumbViewActions);
    thumbView->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void MW::addMenuSeparator(QWidget *widget)
{
    QAction *separator = new QAction(this);
    separator->setSeparator(true);
    widget->addAction(separator);
}

// req'd?
void MW::enableEjectUsbMenu(QString path)
{
    if (G::isLogger) G::log("MW::enableEjectUsbMenu");
//    if(Usb::isUsb(path)) ejectAction->setEnabled(true);
//    else ejectAction->setEnabled(false);
}

void MW::renameEjectUsbMenu(QString path)
{
    QString drive = "";
    QString text;
    bool enabled;
    if (Usb::isEjectable(path)) {
        //qDebug() << "MW::renameEjectUsbMenu  enable";
        enabled = true;
        drive = Utilities::getDriveName(path);
        drive = Utilities::enquote(drive);
        text = "Eject Memory Card " + drive;
    }
    else {
        enabled = false;
        text = "Eject Memory Card";
    }
    //qDebug() << "MW::renameEjectUsbMenu" << path << drive;
    ejectAction->setEnabled(enabled);
    ejectActionFromContextMenu->setEnabled(enabled);
    ejectAction->setText(text);
    ejectActionFromContextMenu->setText(text);
}

void MW::renamePasteFilesMenu(QString folderName)
{
    if (QGuiApplication::clipboard()->mimeData()->hasUrls()) {
        QString txt = "Paste files into " + Utilities::enquote(folderName);
        pasteFilesAction->setText(txt);
        pasteFilesAction->setEnabled(true);
    }
    else {
        pasteFilesAction->setText("Paste files");
        pasteFilesAction->setEnabled(false);
    }
}

void MW::renameEraseMemCardFromContextMenu(QString path)
{
    QString drive = "";
    QString text;
    bool enabled;
    if (Usb::isMemCardWithDCIM(path)) {
        //qDebug() << "MW::renameEjectUsbMenu  enable";
        enabled = true;
        drive = Utilities::getDriveName(path);
        drive = Utilities::enquote(drive);
        text = "Erase all images on Memory Card " + drive;
    }
    else {
        enabled = false;
        text = "Erase Memory Card Images";
    }
    //qDebug() << "MW::renameEjectUsbMenu" << path << drive;
    eraseUsbActionFromContextMenu->setEnabled(enabled);
    eraseUsbActionFromContextMenu->setText(text);
}

void MW::enableSelectionDependentMenus()
{
/*
    rgh check if still working
*/
    if (G::isLogger) G::log("MW::enableSelectionDependentMenus");

    bool enable = dm->rowCount() > 0;

    // File menu
    refreshCurrentAction->setEnabled(enable);
    openWithMenu->setEnabled(enable);
    ingestAction->setEnabled(enable);
    revealFileAction->setEnabled(enable);
    revealFileActionFromContext->setEnabled(enable);
    addBookmarkAction->setEnabled(enable);
    reportMetadataAction->setEnabled(enable);
    copyFolderPathFromContextAction->setEnabled(enable);
    copyImagePathFromContextAction->setEnabled(enable);
    renameAction->setEnabled(enable);
    saveAsFileAction->setEnabled(enable);
    reportMetadataAction->setEnabled(enable);

    //Edit menu
    selectAllAction->setEnabled(enable);
    invertSelectionAction->setEnabled(enable);
    shareFilesAction->setEnabled(enable);
    copyFilesAction->setEnabled(enable);
    copyImageAction->setEnabled(enable);
    copyImagePathFromContextAction->setEnabled(enable);
    deleteImagesAction->setEnabled(enable);
    pickAction->setEnabled(enable);
    rejectAction->setEnabled(enable);
    pickUnlessRejectedAction->setEnabled(enable);
    popPickHistoryAction->setEnabled(enable);
    searchTextEditAction->setEnabled(enable);
    ratingsMenu->setEnabled(enable);
    ratingsMenu->setEnabled(enable);
    labelsMenu->setEnabled(enable);
    rotateRightAction->setEnabled(enable);
    rotateLeftAction->setEnabled(enable);
    utilitiesMenu->setEnabled(enable);

    // Go menu
    goMenu->setEnabled(enable);

    // Filter menu
    filterMenu->setEnabled(enable);

    // Sort menu
    sortMenu->setEnabled(enable);

//    // Embellish menu
    //embelExportMenu->setEnabled(enable);
    embelExportMenu->setEnabled(enable);
    embelNewTemplateAction->setEnabled(enable);
    embelReadTemplateAction->setEnabled(enable);
    embelSaveTemplateAction->setEnabled(enable);

    // View menu
    slideShowAction->setEnabled(enable);
    zoomToAction->setEnabled(enable);
    zoomInAction->setEnabled(enable);
    zoomOutAction->setEnabled(enable);
    zoomToggleAction->setEnabled(enable);
    thumbsEnlargeAction->setEnabled(enable);
    thumbsShrinkAction->setEnabled(enable);

    // Help menu
    helpDiagnosticsMenu->setEnabled(enable);
    diagnosticsAllAction->setEnabled(enable);
    diagnosticsCurrentAction->setEnabled(enable);
    diagnosticsMainAction->setEnabled(enable);
    diagnosticsGridViewAction->setEnabled(enable);
    diagnosticsThumbViewAction->setEnabled(enable);
    diagnosticsImageViewAction->setEnabled(enable);
    diagnosticsMetadataAction->setEnabled(enable);
    diagnosticsDataModelAction->setEnabled(enable);
    diagnosticsMetadataCacheAction->setEnabled(enable);
    diagnosticsImageCacheAction->setEnabled(enable);
    diagnosticsEmbellishAction->setEnabled(enable);
}

void MW::loadShortcuts(bool defaultShortcuts)
{
    if (G::isLogger) G::log("MW::loadShortcuts");
    // Add customizable key shortcut actions
    actionKeys[fullScreenAction->objectName()] = fullScreenAction;
    actionKeys[escapeFullScreenAction->objectName()] = escapeFullScreenAction;
    actionKeys[prefAction->objectName()] = prefAction;
    actionKeys[exitAction->objectName()] = exitAction;
    actionKeys[thumbsEnlargeAction->objectName()] = thumbsEnlargeAction;
    actionKeys[thumbsShrinkAction->objectName()] = thumbsShrinkAction;
    //    actionKeys[cutAction->objectName()] = cutAction;
    //    actionKeys[copyAction->objectName()] = copyAction;
//    actionKeys[keyRightAction->objectName()] = keyRightAction;
//    actionKeys[keyLeftAction->objectName()] = keyLeftAction;
//    actionKeys[keyDownAction->objectName()] = keyDownAction;
//    actionKeys[keyUpAction->objectName()] = keyUpAction;
    //    actionKeys[keepTransformAct->objectName()] = keepTransformAct;
    //    actionKeys[keepZoomAct->objectName()] = keepZoomAct;
    //    actionKeys[copyImageAction->objectName()] = copyImageAction;
    //    actionKeys[pasteImageAction->objectName()] = pasteImageAction;
    //    actionKeys[refreshAction->objectName()] = refreshAction;
    //    actionKeys[pasteAction->objectName()] = pasteAction;
    actionKeys[pickAction->objectName()] = pickAction;
    actionKeys[filterPickAction->objectName()] = filterPickAction;
    actionKeys[ingestAction->objectName()] = ingestAction;
    actionKeys[reportMetadataAction->objectName()] = reportMetadataAction;
    actionKeys[slideShowAction->objectName()] = slideShowAction;
//    actionKeys[keyHomeAction->objectName()] = keyHomeAction;
//    actionKeys[keyEndAction->objectName()] = keyEndAction;
    actionKeys[randomImageAction->objectName()] = randomImageAction;
    actionKeys[openAction->objectName()] = openAction;
    actionKeys[zoomOutAction->objectName()] = zoomOutAction;
    actionKeys[zoomInAction->objectName()] = zoomInAction;
    actionKeys[zoomToggleAction->objectName()] = zoomToggleAction;
    actionKeys[rotateLeftAction->objectName()] = rotateLeftAction;
    actionKeys[rotateRightAction->objectName()] = rotateRightAction;
    //    actionKeys[moveRightAct->objectName()] = moveRightAct;
    //    actionKeys[moveLeftAct->objectName()] = moveLeftAct;
    //    actionKeys[copyToAction->objectName()] = copyToAction;
    //    actionKeys[moveToAction->objectName()] = moveToAction;
    //    actionKeys[keepTransformAct->objectName()] = keepTransformAct;
    actionKeys[ratingBadgeVisibleAction->objectName()] = ratingBadgeVisibleAction;
    actionKeys[iconNumberVisibleAction->objectName()] = iconNumberVisibleAction;
    actionKeys[infoVisibleAction->objectName()] = infoVisibleAction;
    //    actionKeys[toggleAllDocksAct->objectName()] = toggleAllDocksAct;
    actionKeys[folderDockVisibleAction->objectName()] = folderDockVisibleAction;
    actionKeys[favDockVisibleAction->objectName()] = favDockVisibleAction;
    actionKeys[filterDockVisibleAction->objectName()] = filterDockVisibleAction;
    actionKeys[metadataDockVisibleAction->objectName()] = metadataDockVisibleAction;
    actionKeys[thumbDockVisibleAction->objectName()] = thumbDockVisibleAction;
    //    actionKeys[windowTitleBarVisibleAction->objectName()] = windowTitleBarVisibleAction;
    //actionKeys[menuBarVisibleAction->objectName()] = menuBarVisibleAction;
    actionKeys[statusBarVisibleAction->objectName()] = statusBarVisibleAction;
    //    actionKeys[toggleIconsListAction->objectName()] = toggleIconsListAction;
    //    actionKeys[allDocksLockAction->objectName()] = allDocksLockAction;

    settings->beginGroup("Shortcuts");
    QStringList groupKeys = settings->childKeys();

    if (groupKeys.size() && !defaultShortcuts)
    {
        if (groupKeys.contains(exitAction->text())) //rgh find a better way
        {
            QMapIterator<QString, QAction *> key(actionKeys);
            while (key.hasNext()) {
                key.next();
                if (groupKeys.contains(key.value()->text()))
                {
                    key.value()->setShortcut(settings->value(key.value()->text()).toString());
                    settings->remove(key.value()->text());
                    settings->setValue(key.key(), key.value()->shortcut().toString());
                }
            }
        }
        else
        {
            for (int i = 0; i < groupKeys.size(); ++i)
            {
                if (actionKeys.value(groupKeys.at(i)))
                    actionKeys.value(groupKeys.at(i))->setShortcut
                        (settings->value(groupKeys.at(i)).toString());
            }
        }
    }
    else    // default shortcuts
    {
        //    formats to set shortcut
        //    keyRightAction->setShortcut(QKeySequence("Right"));
        //    keyRightAction->setShortcut((Qt::Key_Right);

        //        cutAction->setShortcut(QKeySequence("Ctrl+X"));
        //        copyAction->setShortcut(QKeySequence("Ctrl+C"));
        //        copyImageAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
        //        pasteImageAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
        //        refreshAction->setShortcut(QKeySequence("Ctrl+F5"));
        //        pasteAction->setShortcut(QKeySequence("Ctrl+V"));

        // File
        openAction->setShortcut(QKeySequence("O"));
        refreshCurrentAction->setShortcut(QKeySequence("Alt+F5"));
        openUsbAction->setShortcut(QKeySequence("Ctrl+O"));
        ingestAction->setShortcut(QKeySequence("Q"));
        showImageCountAction->setShortcut(QKeySequence("\\"));
        combineRawJpgAction->setShortcut(QKeySequence("Alt+J"));
        subFoldersAction->setShortcut(QKeySequence("B"));
        revealFileAction->setShortcut(QKeySequence("Ctrl+R"));
        refreshFoldersAction->setShortcut(QKeySequence("F5"));
        collapseFoldersAction->setShortcut(QKeySequence("Alt+C"));
        reportMetadataAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+M"));
        diagnosticsCurrentAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+D"));
        exitAction->setShortcut(QKeySequence("Ctrl+Q"));

        // Edit
        selectAllAction->setShortcut(QKeySequence("Ctrl+A"));
        invertSelectionAction->setShortcut(QKeySequence("Shift+Ctrl+A"));
        rejectAction->setShortcut(QKeySequence("X"));
        pickAction->setShortcut(QKeySequence("`"));
        pickUnlessRejectedAction->setShortcut(QKeySequence("Shift+Ctrl+`"));
        pick1Action->setShortcut(QKeySequence("P"));
        popPickHistoryAction->setShortcut(QKeySequence("Alt+Ctrl+Z"));

        searchTextEditAction->setShortcut(QKeySequence("F2"));

        rate1Action->setShortcut(QKeySequence("1"));
        rate2Action->setShortcut(QKeySequence("2"));
        rate3Action->setShortcut(QKeySequence("3"));
        rate4Action->setShortcut(QKeySequence("4"));
        rate5Action->setShortcut(QKeySequence("5"));

        label1Action->setShortcut(QKeySequence("6"));
        label2Action->setShortcut(QKeySequence("7"));
        label3Action->setShortcut(QKeySequence("8"));
        label4Action->setShortcut(QKeySequence("9"));
        label5Action->setShortcut(QKeySequence("0"));

        rotateLeftAction->setShortcut(QKeySequence("Ctrl+["));
        rotateRightAction->setShortcut(QKeySequence("Ctrl+]"));

        prefAction->setShortcut(QKeySequence("Ctrl+,"));

        // Go
//        keyRightAction->setShortcut(QKeySequence("Right"));
//        keyLeftAction->setShortcut(QKeySequence("Left"));
//        keyHomeAction->setShortcut(QKeySequence("Home"));
//        keyEndAction->setShortcut(QKeySequence("End"));
//        keyDownAction->setShortcut(QKeySequence("Down"));
//        keyUpAction->setShortcut(QKeySequence("Up"));
//        keyPageUpAction->setShortcut(QKeySequence("PgUp"));
//        keyPageDownAction->setShortcut(QKeySequence("PgDown"));

        jumpAction->setShortcut(QKeySequence("J"));

        keyScrollLeftAction->setShortcut(QKeySequence("Alt+Left"));
        keyScrollRightAction->setShortcut(QKeySequence("Alt+Right"));
        keyScrollUpAction->setShortcut(QKeySequence("Alt+Up"));
        keyScrollDownAction->setShortcut(QKeySequence("Alt+Down"));

        keyScrollPageLeftAction->setShortcut(QKeySequence("Ctrl+Alt+Left"));
        keyScrollPageRightAction->setShortcut(QKeySequence("Ctrl+Alt+Right"));
        keyScrollPageUpAction->setShortcut(QKeySequence("Ctrl+Alt+Up"));
        keyScrollPageDownAction->setShortcut(QKeySequence("Ctrl+Alt+Down"));

        nextPickAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+Right"));
        prevPickAction->setShortcut(QKeySequence("Ctrl+Shift+Alt+Left"));
        randomImageAction->setShortcut(QKeySequence("Shift+Ctrl+Right"));

        // Filters
        filterUpdateAction->setShortcut(QKeySequence("Shift+F"));
        clearAllFiltersAction->setShortcut(QKeySequence("Shift+C"));
        filterPickAction->setShortcut(QKeySequence("Shift+`"));

        filterSearchAction->setShortcut(QKeySequence("Shift+S"));

        filterRating1Action->setShortcut(QKeySequence("Shift+1"));
        filterRating2Action->setShortcut(QKeySequence("Shift+2"));
        filterRating3Action->setShortcut(QKeySequence("Shift+3"));
        filterRating4Action->setShortcut(QKeySequence("Shift+4"));
        filterRating5Action->setShortcut(QKeySequence("Shift+5"));

        filterRedAction->setShortcut(QKeySequence("Shift+6"));
        filterYellowAction->setShortcut(QKeySequence("Shift+7"));
        filterGreenAction->setShortcut(QKeySequence("Shift+8"));
        filterBlueAction->setShortcut(QKeySequence("Shift+9"));
        filterPurpleAction->setShortcut(QKeySequence("Shift+0"));

        filterLastDayAction->setShortcut(QKeySequence("Shift+D"));

        // Sort
        sortReverseAction->setShortcut(QKeySequence("alt+S"));

        // View
        asLoupeAction->setShortcut(QKeySequence("E"));
        asGridAction->setShortcut(QKeySequence("G"));
        asTableAction->setShortcut(QKeySequence("T"));
        asCompareAction->setShortcut(QKeySequence("C"));

        slideShowAction->setShortcut(QKeySequence("S"));
        fullScreenAction->setShortcut(QKeySequence("F"));
        //        escapeFullScreenAction->setShortcut(QKeySequence("Esc")); // see MW::eventFilter

        ratingBadgeVisibleAction->setShortcut(QKeySequence("Ctrl+I"));
        iconNumberVisibleAction->setShortcut(QKeySequence("Ctrl+Shift+I"));
        infoVisibleAction->setShortcut(QKeySequence("I"));

        zoomToAction->setShortcut(QKeySequence("Z"));
        zoomInAction->setShortcut(QKeySequence("+"));
        zoomOutAction->setShortcut(QKeySequence("-"));
        //        zoomToggleAction->setShortcut(QKeySequence("Space"));
        zoomToggleAction->setShortcut(Qt::Key_Space);

        //        thumbsFitAction->setShortcut(QKeySequence("Alt+]"));
        thumbsEnlargeAction->setShortcut(QKeySequence("]"));
        thumbsShrinkAction->setShortcut(QKeySequence("["));


        // Window
        newWorkspaceAction->setShortcut(QKeySequence("W"));
        manageWorkspaceAction->setShortcut(QKeySequence("Ctrl+W"));
        defaultWorkspaceAction->setShortcut(QKeySequence("Ctrl+Shift+W"));

        folderDockVisibleAction->setShortcut(QKeySequence("Shift+F3"));
        favDockVisibleAction->setShortcut(QKeySequence("Shift+F4"));
        filterDockVisibleAction->setShortcut(QKeySequence("Shift+F5"));
        metadataDockVisibleAction->setShortcut(QKeySequence("Shift+F6"));
        thumbDockVisibleAction->setShortcut(QKeySequence("Shift+F7"));
        //menuBarVisibleAction->setShortcut(QKeySequence("Shift+F9"));
        statusBarVisibleAction->setShortcut(QKeySequence("Shift+F10"));

        //        folderDockLockAction->setShortcut(QKeySequence("Shift+Alt+F3"));
        //        favDockLockAction->setShortcut(QKeySequence("Shift+Alt+F4"));
        //        filterDockLockAction->setShortcut(QKeySequence("Shift+Alt+F5"));
        //        metadataDockLockAction->setShortcut(QKeySequence("Shift+Alt+F6"));
        //        thumbDockLockAction->setShortcut(QKeySequence("Shift+Alt+F7"));
        //        allDocksLockAction->setShortcut(QKeySequence("Ctrl+L"));

        // Help
        helpAction->setShortcut(QKeySequence("?"));
        //        toggleIconsListAction->setShortcut(QKeySequence("Ctrl+T"));
    }

    settings->endGroup();
}

