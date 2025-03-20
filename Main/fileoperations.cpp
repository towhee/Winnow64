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
    G::popUp->showPopup(msg, 2000);
}

void MW::pasteFiles(QString folderPath)
{
    if (G::isLogger) G::log("MW::pasteFiles");

    // are there any files to paste?
    if (!Utilities::clipboardHasUrls()) {
        QString msg = "There are no files in the clipboard.";
        G::popUp->showPopup(msg, 2000);
        return;
    }

    if (folderPath == "") {
        int n = dm->folderList.count();
        QString nStr = QString::number(n);
        if (n == 0) {
            QString msg = "No folder selected, paste cancelled.";
            G::popUp->showPopup(msg, 2000);
            return;
        }
        if (n > 1) {
            QString msg = "More than 1 folder selected, paste cancelled.";
            G::popUp->showPopup(msg, 2000);
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

    // update folder counts
    fsTree->updateCount(folderPath);
    bookmarks->updateCount();

    // refresh folder to show pasted images rgh check this
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
    G::popUp->showPopup(msg, 2000);
}

void MW::copyFolderPathFromContext()
{
    if (G::isLogger) G::log("MW::copyFolderPathFromContext");
    QApplication::clipboard()->setText(mouseOverFolderPath);
    QString msg = "Copied " + mouseOverFolderPath + " to the clipboard";
    G::popUp->showPopup(msg, 1500);
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
    G::popUp->showPopup(msg, 1500);
}

void MW::renameSelectedFiles()
{
    // rgh redo to make folder agnostic (change RenameFileDlg?)
    QString folderPath = dm->folderList.at(0);
    QStringList selection;
    if (!dm->getSelection(selection)) return;
    // Check all files are in the same folder
    for (int i = 0; i < selection.size(); i++) {
        QString thisFolder = QFileInfo(selection.at(i)).path();
        if (thisFolder != folderPath) {
            QPixmap pixmap(":/images/icon16/subfolders.png");
            QString includesubfoldersIcon = Utilities::pixmapToString(pixmap);
            QString msg = "You can only rename images from a single folder.  If<br>"
                          "'Include subfolders' was invoked you may have selected<br>"
                          "images from multiple folders and there should be a<br>"
                          + includesubfoldersIcon +
                          " in the status bar.<p>"
                          "Press <font color=\"red\"><b>Spacebar</b></font> to continue."
                ;
            G::popUp->showPopup(msg, 0, true, 0.75, Qt::AlignLeft);
            return;
        }
    }

    RenameFileDlg rf(this, folderPath, selection, filenameTemplates, dm, metadata, imageCache);
    rf.exec();
    // may have renamed current image
    setWindowTitle(winnowWithVersion + "   " + dm->currentFilePath);
}

void MW::shareFiles()
{
/*
    Mac only.
*/
#ifdef Q_OS_MAC
    if (G::isLogger) G::log("MW::copy");

    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    QList<QUrl> urls;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        urls << QUrl::fromLocalFile(fPath);
    }

    WId wId = window()->winId();

    Mac::share(urls, wId);
#endif
}

void MW::saveAsFile()
{
    if (G::isLogger) G::log("MW::saveAsFile");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.length() == 0) {
        G::popUp->showPopup("No images selected for save as operation", 1500);
        return;
    }
    saveAsDlg = new SaveAsDlg(selection, metadata, dm);
    saveAsDlg->setStyleSheet(css);
    saveAsDlg->exec();
}

void MW::dmInsert(QStringList pathList)
{
    QString src = "MW::dmInsert";
    // if (G::isLogger)
        G::log(src);
    foreach(QString fPath, pathList) {
        // replace existing image with the same name
        if (dm->isPath(fPath)) {
            qDebug() << src << "replace" << fPath;
            int dmRow = dm->rowFromPath(fPath);
            int sfRow = dm->proxyRowFromPath(fPath);
            // insertedRows << dmRow;
            QModelIndex dmIdx = dm->index(dmRow, G::MetadataLoadedColumn);
            dm->setData(dmIdx, false);
            dm->setIcon(dmIdx, QPixmap(), false, dm->instance, "MW::insert");
            imageCache->removeCachedImage(fPath);
            if (dm->sf->index(sfRow, G::IsCachedColumn).data().toBool()) {
                emit setValueSf(dm->sf->index(sfRow, G::IsCachedColumn), false, instance, src);
            }
            if (dm->sf->index(sfRow, G::IsCachingColumn).data().toBool()) {
                emit setValueSf(dm->sf->index(sfRow, G::IsCachingColumn), false, instance, src);
            }
            if (dm->sf->index(sfRow, G::AttemptsColumn).data().toInt()) {
                emit setValueSf(dm->sf->index(sfRow, G::AttemptsColumn), 0, instance, src);
            }
            G::allMetadataLoaded = false;
            G::iconChunkLoaded = false;
        }
        // insert a new image
        else {
            qDebug() << src << "insert" << fPath;
            dm->insert(fPath);
            ImageMetadata m = dm->imMetadata(fPath, false);
            // filters->save();
            // clearAllFilters();
            // dm->sf->suspend(false, "MW::filterChange");
            // dm->sf->filterChange("MW::filterChange");  // crash (removed wait in SortFilter::filterChange)
            buildFilters->rebuild();
            // filters->restore();
            sel->select(dm->currentFilePath);
        }
    }
    // imageView->currentImageHasChanged = true;
    // rebuild the filters to include new or changed images
    // buildFilters->build();
    // filterChange(src);
    // updateAllFilters();
    // update current to account for insertions
    // dm->setCurrent(dm->currentFilePath, dm->instance);
    // selection triggers thumbnail and imagecache updates
    // sel->select(dm->currentFilePath);
}

void MW::insertFiles(QStringList pathList)
{
/*
    Replace or insert a new image file into the datamodel.

    After insertion, the call function should select row: sel->select(fPath);
    This will invoke MetaRead which will load the metadata, icon and imageCache.
*/
    if (G::isLogger) G::log("MW::insertFile", "dm->instance = " + QString::number(dm->instance));

    if (pathList.isEmpty()) {
        QString msg = "No files to insert, fPaths is empty.";
        G::issue("Warning", msg, "MW::insertFiles");
        return;
    }

    QString fPath;
    QList<int> insertedRows;
    int dmRow;
    QString src = "MW::insertFiles";

    // must sort fPaths before insertion in case multiple items are appended to end of datamodel
    // fPaths.sort(Qt::CaseInsensitive);
    dmInsert(pathList);
}

void MW::deleteSelectedFiles()
{
/*
    Build a QStringList of the selected files and call MW::deleteFiles.
*/
    if (G::isLogger) G::log("MW::deleteSelectedFiles");
    // make sure datamodel is loaded
    if (!G::allMetadataLoaded) {
        QString msg = "Please wait until the folder has been completely loaded<br>"
                      "before deleting images.  When the folder is completely<br>"
                      "loaded the metadata light in the status bar (2nd from the<br>"
                      "right side) will turn green.<p>"
                      "Press <font color=\"red\"><b>Spacebar</b></font> to continue.";
        G::popUp->showPopup(msg, 0);
        return;
    }

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
        QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                    "QPushButton:default {background-color: rgb(68,95,118);}";
        msgBox.setStyleSheet(css);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        msgBox.show();
        QPoint pos = geometry().center() + QPoint(0, 300);
        /*
        qDebug() << "MW::deleteSelectedFiles before move"
                 << "msgBox =" << msgBox.pos()
                 << "geometry().topLeft() =" << geometry().topLeft()
                 << "geometry().center() =" << geometry().center()
                 << "geometry() =" << geometry()
                 << "pos =" << pos
            ; //*/
        msgBox.move(geometry().center());
//        msgBox.move(QPoint(0,0));
        int ret = msgBox.exec();
        resetFocus();
        if (ret == QMessageBox::Cancel) return;
    }

    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    // convert selection to stringlist
    QStringList paths;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::PathRole).toString();
        paths.append(fPath);
    }

    deleteFiles(paths);
}

void MW::dmRemove(QStringList pathList)
{
    /*
    Remove the images from the datamodel.  This must be done while the proxymodel dm->sf
    is the same as dm: no filtering.  We save the filter, clear filters, remove all the
    datamodel rows matching the image fPaths and restore the filter.  dm->remove deletes
    the rows, updates dm->fPathRow.
    */
    qDebug() << "MW::dmRemove(QStringList pathList)" << pathList;

    filters->save();
    clearAllFilters();

    for (int i = 0; i < pathList.count(); ++i) {
        QString fPath = pathList.at(i);
        qDebug() << "MW::dmRemove" << pathList.at(i);
        dm->remove(fPath);
    }

    // cleanup G::rowsWithIcon
    metaRead->cleanupIcons();

    // remove deleted files from imageCache
    imageCache->removeFromCache(pathList);

    G::ignoreScrollSignal = false;

    // rebuild filters
    buildFilters->rebuild();
    filters->restore();
}

void MW::deleteFiles(QStringList paths)
{
/*
    Delete from disk, remove from datamodel, remove from ImageCache and update the
    image cache status bar.
*/
    if (G::isLogger) G::log("MW::deleteFiles");
    QElapsedTimer t;
    t.restart();

    // if still loading metadata then do not delete
    if (!G::allMetadataLoaded) {
        QString msg = "Please wait until the folder has been completely loaded<br>"
                      "before deleting images.  When the folder is completely<br>"
                      "loaded the metadata light in the status bar (2nd from the<br>"
                      "right side) will turn green.<p>"
                      "Press <font color=\"red\"><b>Spacebar</b></font> to continue.";
        G::popUp->showPopup(msg, 0);
        return;
    }

    /* save the index to the first row in selection (order depends on how selection was
       made) to insure the correct index is selected after deletion.  */
    int lowRow = 999999;
    if (dm->sf->rowCount() == 1) {
        lowRow = dm->rowFromPath(paths.at(0));
    }
    else {
        for (int i = 0; i < paths.count(); ++i) {
            int row = dm->proxyRowFromPath(paths.at(i));
            if (row < lowRow) {
                lowRow = row;
            }
        }
    }

    G::ignoreScrollSignal = true;

    // delete file(s) in folder on disk, including any xmp sidecars
    QStringList sldm;       // paths of successfully deleted files to remove in datamodel
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
                sldm.append(fPath);
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
    if (fileWasLocked) G::popUp->showPopup("Locked file(s) were not deleted", 3000);

    // if all images in folder were deleted
    // if (sldm.count() == dm->sf->rowCount()) {
    //     bookmarks->updateBookmarks();
    //     stop("deleteFiles");
    //     // fsTree->select(dm->currentPrimaryFolderPath);
    //     return;
    // }

    dmRemove(sldm);

    // update current index
    if (dm->sf->rowCount()) {
        int sfRow;
        if (lowRow >= dm->sf->rowCount()) lowRow = dm->sf->rowCount() - 1;
        sfRow = dm->nearestProxyRowFromDmRow(dm->modelRowFromProxyRow(lowRow));

        QModelIndex sfIdx = dm->sf->index(sfRow, 0);
        sel->select(sfIdx, Qt::NoModifier,"MW::deleteFiles");
    }

    // /* Just in case deletion was from a bookmark folder then force update for image count.
    //    This is required if fsTree is hidden, and consequently, is not receiving signals.
    //    As a result, fsTree does not update its data and does not signal back to Bookmarks. */
    fsTree->updateCount();
    bookmarks->updateCount();
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
    if (G::isLogger) G::log("MW::deleteFolder");
    QString dirToDelete;
    QString senderObject = (static_cast<QAction*>(sender()))->objectName();
    // rgh allow deletion of multiple folders with ample warnings
    if (senderObject == "deleteActiveFolder") {
        dirToDelete = dm->folderList.at(0);
    }
    else if (senderObject == "deleteFSTreeFolder") {
        dirToDelete = mouseOverFolderPath;
    }

    if (!QFile(dirToDelete).exists()) {
        QString msg = dirToDelete + " does not exist";
        G::popUp->showPopup(msg, 2000);
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
                       "<br>to the " + trash);
        msgBox.setInformativeText("Do you want continue?");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        QString s = "QWidget{font-size: 12px; background-color: rgb(85,85,85); color: rgb(229,229,229);}"
                    "QPushButton:default {background-color: rgb(68,95,118);}";
        msgBox.setStyleSheet(s);
        msgBox.setStyleSheet(css);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        int ret = msgBox.exec();
        resetFocus();
        if (ret == QMessageBox::Cancel) return;
    }

    QModelIndex currIdx = fsTree->currentIndex();

    if (dm->folderList.contains(dirToDelete)) {
        stop("deleteFolder");
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

    // show msg if this wsa the active folder and there are no images now
    if (dm->folderList.contains(dirToDelete)) {
        if (dm->folderList.count() == 1) {
            // stop();
            // reset();
            dm->clear();
            dm->clearAllIcons();
            setCentralMessage(dirToDelete + "\n has been sent to the " + G::trash);
        }
        // if (dm->folderList.count() > 1) {
        //     folderSelectionChange(dirToDelete, "Remove", false, false);
        // }
    }
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
    G::popUp->showPopup(msg);

    // rgh check what to do

    // if (G::currRootFolder.contains(rootPath)) {
    //     fsTree->select(rootPath);
    // }
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
                if (Usb::isUsb(storage.rootPath())) {
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
