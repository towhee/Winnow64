#include "Main/mainwindow.h"

/*  *******************************************************************************************


*/

void MW::copyFiles()
{
    if (G::isLogger) G::log("MW::copy");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    bool isSidecar = false;
    int n = selection.count();
    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;

    for (int i = 0; i < n; ++i) {
        // add image path
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
        // add sidecar path(s)
        QStringList sidecarPaths = Utilities::getSidecarPaths(fPath);
        foreach (QString sidecarPath, sidecarPaths) {
            urls << QUrl::fromLocalFile(sidecarPath);
            isSidecar = true;
        }
    }
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    // popup msg
    QString nPaths;
    if (n == 1) nPaths = "1 image file";
    else nPaths = QString::number(n) + " image files";
    QString msg = "Copied " + nPaths + " to the clipboard";
    if (isSidecar) msg += " including associated sidecar files";
    G::popup->showPopup(msg, 2000);
}

void MW::pasteFiles(QString folderPath)
{
    if (G::isLogger) G::log("MW::pasteFiles");

    // are there any files to paste?
    if (!Utilities::clipboardHasUrls()) {
        QString msg = "There are no files in the clipboard.";
        G::popup->showPopup(msg, 2000);
        return;
    }

    if (folderPath == "") {
        int n = dm->folderList.count();
        QString nStr = QString::number(n);
        if (n == 0) {
            QString msg = "No folder selected, paste cancelled.";
            G::popup->showPopup(msg, 2000);
            return;
        }
        if (n > 1) {
            QString msg = "More than 1 folder selected, paste cancelled.";
            G::popup->showPopup(msg, 2000);
            return;
        }

        folderPath = dm->folderList.at(0);
    }

    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    QList<QUrl> urls;
    QStringList newPaths;
    foreach (QUrl url, mimeData->urls()) {
        qDebug() << url.path();
        QString srcPath = url.path();
        QFileInfo fi(url.path());
        if (QFile(srcPath).exists()) {
            QString destPath = folderPath + "/" + fi.fileName();
            QFile::copy(url.path(), destPath);
            newPaths << destPath;
        }
    }

    // dynamically update datamodel etc
    int imageCount = 0;
    int sidecarCount = 0;
    foreach (QString path, newPaths) {
        if (metadata->supportedFormats.contains(Utilities::getSuffix(path))) {
            // insertFile(path);   // rgh_insert
            imageCount++;
        }
        else sidecarCount++;
    }
    // update filter counts
    buildFilters->recount();
    // updateFilterMenu("MW::pasteFiles");

    // update folder counts
    fsTree->updateAFolderCount(folderPath);
    bookmarks->updateCount();

    // refresh folder to show pasted images
    if (dm->folderList.contains(folderPath)) {
        folderAndFileSelectionChange(dm->currentFilePath, "pasteFiles");
    }

    // popup msg
    QString mImages;
    QString mSidecars = "";
    if (imageCount == 1) mImages = "1 image file";
    else mImages = QString::number(imageCount) + " image files";
    if (sidecarCount == 1) mSidecars = " and 1 sidecar file";
    if (sidecarCount > 1) mSidecars = " and " + QString::number(sidecarCount) + " sidecar files";
    QString msg = "Pasted " + mImages + mSidecars;
    G::popup->showPopup(msg, 2000);
}

void MW::copyFolderPathFromContext()
{
    if (G::isLogger) G::log("MW::copyFolderPathFromContext");
    QApplication::clipboard()->setText(mouseOverFolderPath);
    QString msg = "Copied " + mouseOverFolderPath + " to the clipboard";
    G::popup->showPopup(msg, 1500);
}

void MW::copyImagePathFromContext()
{
    if (G::isLogger) G::log("MW::copyImagePathFromContext");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    int n = selection.count();
    QString paths;
    for (int i = 0; i < n; ++i) {
        paths += selection.at(i).data(G::PathRole).toString();
        if (i < n - 1) paths += "\n";
    }
    QApplication::clipboard()->setText(paths);

    QString nPaths;
    if (n == 1) nPaths = "1 path";
    else nPaths = QString::number(n) + " paths";
    QString msg = "Copied " + nPaths + " to the clipboard";
    G::popup->showPopup(msg, 1500);
}

void MW::renameSelectedFiles()
{
    QString folderPath = dm->folderList.at(0);
    QStringList selection;
    if (!dm->getSelectionOrPicks(selection)) return;
    /* getSelection can return true with an empty list (no picks, no selection);
    opening the dialog in that state crashes when updateExample touches
    selection.at(0) via the combobox's currentTextChanged signal. */
    if (selection.isEmpty()) {
        G::popup->showPopup("No images selected to rename.", 2000);
        return;
    }

    // Check all files are in the same folder
    for (int i = 0; i < selection.size(); i++) {
        QString thisFolder = QFileInfo(selection.at(i)).path();
        if (thisFolder != folderPath) {
            QString msg = "You can only rename images from a single folder.<p>"
                          "Press <font color=\"red\"><b>Esc</b></font> to continue.";
            G::popup->showPopup(msg, 0, true, 0.75, Qt::AlignLeft);
            return;
        }
    }

    // Pre-check for Finder-locked files (macOS UF_IMMUTABLE). rename() fails
    // with EPERM on such files, so warn and bail before opening the dialog —
    // the user has to unlock in Finder first.
    QStringList lockedFiles = RenameFileDlg::lockedFilesInSelection(folderPath, selection);
    if (!lockedFiles.isEmpty()) {
        QMessageBox::warning(this, "File locked",
                             RenameFileDlg::lockedFilesMsg(lockedFiles));
        return;
    }

    RenameFileDlg rf(this, folderPath, selection, filenameTemplates,
                     dm, metadata, imageCache);
    rf.exec();

    // may have renamed current image
    setWindowTitle(winnowWithVersion + "   " + dm->currentFilePath);
}

void MW::shareFiles()
{
/*
    Raise the OS share UI for the selected images: the macOS share sheet
    (Mac::share) or the Windows Share flyout (Win::share).
*/
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    if (G::isLogger) G::log("MW::shareFiles");

    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    QList<QUrl> urls;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
    }

    WId wId = window()->winId();

  #if defined(Q_OS_MAC)
    Mac::share(urls, wId);
  #else
    Win::share(urls, wId);
  #endif
#endif
}

void MW::saveAsFile()
{
    if (G::isLogger) G::log("MW::saveAsFile");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.length() == 0) {
        G::popup->showPopup("No images selected for save as operation", 1500);
        return;
    }
    saveAsDlg = new SaveAsDlg(selection, metadata, dm);
    saveAsDlg->setStyleSheet(G::css);
    if (saveAsDlg->exec() == QDialog::Accepted) {
        QString savedFolderPath = saveAsDlg->getFolderPath();
        qDebug() << "MW::saveAsFile"
                 << "savedFolderPath =" << savedFolderPath
                 << "dm->folderList =" << dm->folderList
            ;
        if (dm->folderList.contains(savedFolderPath)) {
            qDebug() << "User saved to:" << savedFolderPath << "Do something";
            refresh();
        }
    }

    fsTree->updateCount();
    bookmarks->updateCount();
}

void MW::dmInsert(QStringList pathList)
{
    QString src = "MW::dmInsert";
    if (G::isLogger)
        G::log(src);
    foreach(QString fPath, pathList) {
        // replace existing image with the same name
        if (dm->isPath(fPath)) {
            int dmRow = dm->rowFromPath(fPath);
            int sfRow = dm->proxyRowFromPath(fPath, src);
            // qDebug() << src << "replace row" << sfRow << fPath;
            // insertedRows << dmRow;
            QModelIndex dmIdx = dm->index(dmRow, G::MetadataStatusColumn);
            dm->setData(dmIdx, G::MetaNotAttempted);
            dm->setIcon(dmIdx, QPixmap(), dm->instance, "MW::insert");
            imageCache->removeCachedImage(fPath);
            if (dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()) {
                emit setValSf(sfRow, G::IsCachedColumn, false, instance, src);
            }
            if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool()) {
                emit setValSf(sfRow, G::IsCachingColumn, false, instance, src);
            }
            if (dm->sf->index(sfRow, G::AttemptsColumn).data().toInt()) {
                emit setValSf(sfRow, G::AttemptsColumn, 0, instance, src);
            }
            G::allMetadataAttempted = false;
            G::iconChunkLoaded = false;
        }
        // insert a new image
        else {
            qDebug() << src << "insert" << fPath;
            dm->insert(fPath);
            ImageMetadata m = dm->imMetadata(fPath, false);
            buildFilters->rebuild();
            sel->select(dm->currentFilePath);
        }
    }
}

void MW::insertFiles(QStringList pathList)
{
/*
    Replace or insert a new image file into the datamodel.

    After insertion, the call function should select row: sel->select(fPath);
    This will invoke MetaRead which will load the metadata, icon and imageCache.

    It is used by a remote embellish operation when the embellish folder is in the
    datamodel.
*/
    if (G::isLogger) G::log("MW::insertFile", "dm->instance = " + QString::number(dm->instance));

    if (pathList.isEmpty()) {
        QString msg = "No files to insert, fPaths is empty.";
        G::issue("Warning", msg, "MW::insertFiles");
        return;
    }

    QString fPath;
    QList<int> insertedRows;
    QString src = "MW::insertFiles";

    // must sort fPaths before insertion in case multiple items are appended to end of datamodel
    // fPaths.sort(Qt::CaseInsensitive);
    dmInsert(pathList);

    // updata datamodel, imagecache, image counts, selection
    refresh();

    /* Re-read metadata + thumbnail synchronously for the inserted/replaced files
       so each row ends MetaLoaded with a current icon. For a replaced file (same
       path, new content - e.g. an image re-embellished in place, or a re-run
       focus stack), dm->refresh() inside refresh() above detects it as "modified"
       and resets it to MetaNotAttempted, expecting an async metaRead re-read.
       That re-read does not reliably complete in this synchronous insert flow,
       and the ImageCache only decodes MetaLoaded rows - so the image stayed blank
       in the loupe (and its thumbnail stale) until the user navigated away.
       Loading here (after refresh, before the caller selects) makes the
       subsequent selection decode and display the image, and refreshes the icon. */
    if (!refreshThumb && metaRead)
        refreshThumb = new Thumb(dm, metaRead->getFrameDecoder());

    for (const QString &fPath : pathList) {
        int dmRow = dm->rowFromPath(fPath);
        if (dmRow < 0) continue;
        // only the rows reset by dm->refresh() (modified/new) need reloading
        if (dm->index(dmRow, G::MetadataStatusColumn).data().toInt() == G::MetaLoaded)
            continue;

        QFileInfo fileInfo(fPath);
        if (!metadata->loadImageMetadata(fileInfo, dmRow, dm->instance,
                                         true, true, false, true, src))
            continue;
        dm->addMetadataForItem(metadata->m, src);

        /* Refresh the thumbnail. setIcon is idempotent and will not replace a
           live icon, so clear the stale decoration first, then load the new
           embedded thumb (metadata->m, just read above, has its offset/length). */
        if (refreshThumb) {
            QModelIndex iconIdx = dm->index(dmRow, 0);
            dm->setData(iconIdx, QVariant(), Qt::DecorationRole);
            QString p = fPath;
            QImage image;
            if (refreshThumb->loadThumb(p, dmRow, image, dm->instance, metadata->m, src)) {
                QImage icon = image.scaled(G::maxIconSize, G::maxIconSize,
                                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
                dm->setIcon(iconIdx, QPixmap::fromImage(icon), dm->instance, src);
            }
        }
    }

    /* Re-assert the proxy's sorted order. dm->insert() placed each new row at its
       sorted position in the SOURCE model, but with no active proxy sort column
       sf appends it, and the addMetadataForItem dataChanged emitted in the loop
       above re-appends it to the end. filterChange() (invalidateRowsFilter) rebuilds
       the proxy mapping in source order so the inserted rows land in their sorted
       position. This must run synchronously here - the async metadataLoaded signal
       that refresh() relied on does not reliably fire in this insert flow.
       See memory project_insertfiles_proxy_brittle. */
    dm->sf->filterChange(src);
    dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();
    if (thumbView->isVisible()) thumbView->refreshIcons(src);
    if (gridView->isVisible()) gridView->refreshIcons(src);
}

void MW::deleteSelectedFiles()
{
/*
    Build a QStringList of the selected files and call MW::deleteFiles.
*/
    if (G::isLogger) G::log("MW::deleteSelectedFiles");

    // make sure datamodel is loaded
    if (!G::allMetadataAttempted) {
        QString msg = "Please wait until the folder has been completely loaded<br>"
                      "before deleting images.  When the folder is completely<br>"
                      "loaded the metadata light in the status bar (2nd from the<br>"
                      "right side) will turn green.<p>"
                      "Press <font color=\"red\"><b>Esc</b></font> to continue.";
        G::popup->showPopup(msg, 0);
        return;
    }

    // Warning MessageBox
    if (deleteWarning) {
        QMessageBox msgBox(this);
        int msgBoxWidth = 300;
        msgBox.setWindowTitle("Delete Images");
        #ifdef Q_OS_WIN
        msgBox.setText("This operation will move all selected images to the recycle bin.");
        #endif
        #ifdef Q_OS_MAC
        msgBox.setText("This operation will move all selected images to the trash.");
        #endif
        msgBox.setInformativeText("Do you want continue?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(G::css);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        msgBox.show();
        msgBox.move(geometry().center());
        int ret = msgBox.exec();
        resetFocus();
        if (ret == QMessageBox::Cancel) return;
    }

    // QModelIndexList selection = dm->selectionModel->selectedRows();
    // if (selection.isEmpty()) return;
    QModelIndexList selection;
    if (G::mode == "Grid") {
        selection = gridView->selectionModel()->selectedRows();
    } else if (G::mode == "Table") {
        selection = tableView->selectionModel()->selectedRows();
    } else {
        selection = dm->selectionModel->selectedRows();
    }

    if (selection.isEmpty()) {
        G::popup->showPopup("No images selected to delete.", 1500);
        return;
    }

    QStringList paths;
    paths.reserve(selection.size());

    for (const QModelIndex &sfIdx : selection) {
        QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
        QString fPath = dmIdx.data(G::PathRole).toString();
        if (!fPath.isEmpty())
            paths << fPath;
    }

    paths.removeDuplicates();
    deleteFiles(paths);
}

void MW::deleteFiles(QStringList paths)
{
/*
    Delete from disk, remove from datamodel, remove from ImageCache and update the
    image cache status bar.
*/
    if (G::isLogger) G::log("MW::deleteFiles");

    // if still loading metadata then do not delete
    if (!G::allMetadataAttempted) {
        QString msg = "Please wait until the folder has been completely loaded<br>"
                      "before deleting images.  When the folder is completely<br>"
                      "loaded the metadata light in the status bar (2nd from the<br>"
                      "right side) will turn green.<p>"
                      "Press <font color=\"red\"><b>Esc</b></font> to continue.";
        G::popup->showPopup(msg, 0);
        return;
    }

    G::ignoreScrollSignal = true;

    // delete file(s) in folder on disk, including any xmp sidecars
    bool fileWasLocked = false;
    for (int i = 0; i < paths.count(); ++i) {
        QString fPath = paths.at(i);
        if (QFile::exists(fPath)) {
            // delete the file
            ImageMetadata m = dm->imMetadata(fPath);
            if (!m.isReadWrite) {
                fileWasLocked = true;
                QString msg = "File is locked.";
                G::issue("Warning", msg, "MW::deleteFiles", -1, fPath);
            }
            if (QFile(fPath).moveToTrash()) {
                QStringList srcSidecarPaths = Utilities::getSidecarPaths(fPath);
                foreach (QString sidecarPath, srcSidecarPaths) {
                    if (QFile(sidecarPath).exists()) {
                        QFile(sidecarPath).moveToTrash();
                    }
                }
            }
            else {
                QString msg = "Unable to move to trash.";
                G::issue("Warning", msg, "MW::deleteFiles", -1, fPath);
            }
        }
        else {
            QString msg = "File does not exist.";
            G::issue("Warning", msg, "MW::deleteFiles", -1, fPath);
        }
    }
    if (fileWasLocked) G::popup->showPopup("Locked file(s) were not deleted", 3000);

    // updata datamodel, imagecache, image counts
    refresh();

    /* Update selection. When every filtered item is deleted the filters are
       cleared and the prior current/saved rows no longer exist, leaving
       dm->currentSfRow invalid (-1). Fall back to the first row so a valid
       selection is always set (unless the folder is now empty). */
    int sfRow = dm->currentSfRow;
    if (sfRow < 0 || sfRow >= dm->sf->rowCount()) sfRow = 0;
    if (dm->sf->rowCount() > 0) sel->select(sfRow);
}

void MW::currentFolderDeletedExternally(QString path)
{
    if (G::isLogger) G::log("MW::currentFolderDeletedExternally");
    // qDebug() << "MW::currentFolderDeletedExternally" << path;

    bool isExternalDeletion = path != lastFolderDeletedByWinnow;
    stop();

    // do not highlight next folder
    // fsTree->setCurrentIndex(QModelIndex());

    if (isExternalDeletion) {
        setCentralMessage("The current folder was deleted by an external event.");
    }

    // do not highlight next folder
    fsTree->setCurrentIndex(QModelIndex());
}

void MW::deleteFolder()
{
    if (G::isLogger)
        G::log("MW::deleteFolder");
    QString dirToDelete;
    QString senderObject = (static_cast<QAction*>(sender()))->objectName();
    // allow deletion of multiple folders with ample warnings
    if (senderObject == "deleteActiveFolder") {
        dirToDelete = dm->folderList.at(0);
    }
    else if (senderObject == "deleteFSTreeFolder") {
        dirToDelete = mouseOverFolderPath;
    }

    if (!QFile(dirToDelete).exists()) {
        QString msg = dirToDelete + " does not exist";
        G::popup->showPopup(msg, 2000);
        return;
    }

    if (deleteWarning) {
        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setWindowTitle("Delete Folder");
        msgBox.setTextFormat(Qt::RichText);
        #ifdef Q_OS_WIN
        QString trash = "recycle bin";
        #endif
        #ifdef Q_OS_MAC
        QString trash = "trash";
        #endif
        msgBox.setText("This operation will move the folder<br>"
                       + dirToDelete +
                       "<br>and all subfolders to the " + trash + ".");
        msgBox.setInformativeText("Do you want continue?");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        msgBox.setStyleSheet(G::css);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        int ret = msgBox.exec();
        resetFocus();
        if (ret == QMessageBox::Cancel) return;
    }

    if (dm->folderList.contains(dirToDelete)) {
        stop("deleteFolder");
        // reset();
        setCentralMessage(dirToDelete + "\n has been sent to the " + G::trash);
    }

    // okay to delete
    QFile(dirToDelete).moveToTrash();

    // currentFolderDeletedExternally can check if internal folder deletion
    lastFolderDeletedByWinnow = dirToDelete;

    if (bookmarks->bookmarkPaths.contains(dirToDelete)) {
        bookmarks->bookmarkPaths.remove(dirToDelete);
        bookmarks->reloadBookmarks();
    }

    // do not highlight next folder
    fsTree->setCurrentIndex(QModelIndex());
}

void MW::deleteAllImageMemCard(QString rootPath, QString name)
{
    if (G::isLogger) G::log("MW::deleteAllImageUsbDCIM");

    // ignore if no DCIM folder
    QString dcimPath = rootPath + "/DCIM";
    QDir dcimDir = QDir(dcimPath);
    qDebug() << "MW::deleteAllImageMemCard" << dcimPath;
    if (!dcimDir.exists()) {
        QString msg = "Drive " + name +
                      " does not contain a folder called DCIM.";
        QMessageBox::information(this, "Invalid drive", msg);
        return;
    }

    // warning
    QMessageBox msgBox;
    msgBox.setWindowTitle("Delete All Images on " + name);
    #ifdef Q_OS_WIN
    QString trash = "recycle bin";
    #endif
    #ifdef Q_OS_MAC
    QString trash = "trash";
    #endif
    msgBox.setText("This operation will DELETE ALL\n"
                   "the images on drive\n\n"
                   + name + "\n\n"
                   "The images will NOT be copied to the " + trash);
    msgBox.setInformativeText("Do you want continue?");
    msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    int ret = msgBox.exec();
    resetFocus();
    if (ret == QMessageBox::Cancel) return;

    // delete
    qDebug() << "MW::deleteAllImageMemCard   removed" << dcimPath;
    dcimDir.removeRecursively();

    QString msg = "All images removed from " + name;
    G::popup->showPopup(msg);
}

void MW::eraseMemCardImages()
{
    /*
    A list of available USB drives are listed in a dialog for the user.  For the selected
    drive, all the subfolders in the DCIM folder are deleted.
*/
    if (G::isLogger) G::log("MW::eraseMemCardImages");

    struct  UsbInfo {
        QString rootPath;
        QString name;
        QString description;
    };
    UsbInfo usbInfo;

    QMap<QString, UsbInfo> usbMap;
    QStringList usbDrives;
    int n = 0;
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady()) {
            if (!storage.isReadOnly()) {
                if (UsbUtil::isMemCardWithDCIM(storage.rootPath())) {
                    QString dcimPath = storage.rootPath() + "/DCIM";
                    if (QDir(dcimPath).exists(dcimPath)) {
                        usbInfo.rootPath = storage.rootPath();
                        usbInfo.name = storage.name();
                        QString count = QString::number(n) + ". ";
                        if (usbInfo.name.length() > 0)
                            usbInfo.description = count + usbInfo.name + " (" + usbInfo.rootPath + ")";
                        else
                            usbInfo.description = count + usbInfo.rootPath;
                        usbMap.insert(usbInfo.description, usbInfo);
                        usbDrives << usbInfo.description;
                        n++;
                    }
                }
            }
        }
    }

    // select drive
    QString drive;
    EraseMemCardDlg *deleteUsbDlg = new EraseMemCardDlg(this, usbDrives, drive);
    if (!deleteUsbDlg->exec()) {
        qDebug() << "MW::eraseMemCardImages cancelled";
        return;
    }

    //qDebug() << "MW::eraseMemCardImages" << usbMap[drive].rootPath << usbMap[drive].name;
    deleteAllImageMemCard(usbMap[drive].rootPath, usbMap[drive].name);
}

void MW::eraseMemCardImagesFromContextMenu()
{
    if (G::isLogger) G::log("MW::eraseMemCardImagesFromContextMenu");
    //qDebug() << "MW::eraseMemCardImagesFromContextMenu"
    //         << mouseOverFolderPath;
    deleteAllImageMemCard(mouseOverFolderPath, mouseOverFolderPath);
}
