#include "Main/mainwindow.h"
#include "Utilities/fileops.h"
#include "Cache/devpreviewcache.h"
#include <QSet>
#include <QElapsedTimer>
#include <QProgressDialog>

/*  *******************************************************************************************


*/

/*
    Folder-level refusal for the develop preview cache. FileOps guards the per-file
    operations, but deleting the folder or pasting into it are one step above that, and
    both would take the cache with them. See Cache/devpreviewcache.h.
*/
static bool refuseCacheFolder(const QString &path)
{
    if (!DevPreviewCache::instance().isCachePath(path)) return false;
    if (G::popup)
        G::popup->showPopup("Not allowed: " + DevPreviewCache::readOnlyReason() + ".", 3000);
    return true;
}

void MW::copyFiles()
{
    if (G::isLogger) G::log("MW::copy");
    QModelIndexList selection = dm->selectionModel->selectedRows();
    if (selection.isEmpty()) return;

    // flush unsaved per-image Develop edits so copied sidecars are current
    FileOps::flushPendingEdits();

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
        QStringList sidecarPaths = FileOps::companions(fPath);
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

    if (refuseCacheFolder(folderPath)) return;

    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    QStringList newPaths;

    /* Two passes. MW::copyFiles puts the sidecars on the clipboard as URLs of their own,
       but a clipboard filled by Finder/Explorer holds images only. So route every image
       through FileOps (which carries its companions either way) and then paste only those
       clipboard sidecars FileOps did not already account for -- otherwise an internal
       copy/paste would copy each sidecar twice. */
    QStringList srcImages;
    QStringList srcOthers;
    foreach (const QUrl &url, mimeData->urls()) {
        const QString srcPath = url.toLocalFile();
        if (srcPath.isEmpty() || !QFile::exists(srcPath)) continue;
        if (metadata->supportedFormats.contains(Utilities::getSuffix(srcPath)))
            srcImages << srcPath;
        else
            srcOthers << srcPath;
    }

    QSet<QString> handled;
    foreach (const QString &srcPath, srcImages) {
        const QString destPath = folderPath + "/" + QFileInfo(srcPath).fileName();
        if (!FileOps::copyFile(srcPath, destPath)) continue;
        newPaths << destPath;
        foreach (const QString &c, FileOps::companions(srcPath)) {
            handled.insert(c);
            newPaths << folderPath + "/" + QFileInfo(c).fileName();
        }
    }

    foreach (const QString &srcPath, srcOthers) {
        if (handled.contains(srcPath)) continue;
        const QString destPath = folderPath + "/" + QFileInfo(srcPath).fileName();
        if (QFile::copy(srcPath, destPath)) newPaths << destPath;
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
    /* PROBE copy-path (temporary) */
    if (G::isCopyPathProbe) {
        qDebug().noquote() << "COPYPATH slot entered"
                           << "mouseOverFolderPath =" << mouseOverFolderPath;
    }
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

    /* A rename inside the 2s Develop debounce would otherwise let the pending write
       land afterwards and recreate a sidecar at the OLD name. */
    FileOps::flushPendingEdits();

    RenameFileDlg rf(this, folderPath, selection, filenameTemplates,
                     dm, metadata, imageCache);
    rf.exec();

    // may have renamed current image
    titleFilePath = dm->currentFilePath;
    updateWindowTitle();
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
/*
    File > Save Preview as: write the selected images out WITHOUT a develop recipe -- the
    plain browse decode, which is what "preview" means here.

    This runs through the same ExportDlg / ImageExporter as the Develop export, differing
    only in the pixel source it supplies. It replaces the old SaveAsDlg, whose private
    copy of the export loop offered three formats, no naming control, no resizing, no ICC
    tag and no metadata copy. Mode::Preview hides settings the browse decode cannot honour
    (bit depth), since an 8-bit source cannot deliver a 16-bit file.
*/
    if (G::isLogger) G::log("MW::saveAsFile");

    QStringList targets;
    if (!prepareExport(targets)) return;

    imageExporter->setPixelSource(
        [this](const QString &fPath, ImageExporter::Done done) {
            previewPixelSource(fPath, done);
        });

    ExportDlg dlg(imageExporter, exportPresets, targets, dm->currentFilePath,
                  filenameTemplates, ExportDlg::Mode::Preview, this);
    QMetaObject::Connection c = connect(imageExporter, &ImageExporter::finished, this,
        [this](const ImageExporter::Result &r) {
            onExportFinished(r, imageExporter->activeSettings().addToFolderView);
        });
    dlg.exec();
    disconnect(c);
    if (exportPresets) exportPresets->writeLast(dlg.settings());
}

void MW::applyModelChange(const QStringList &added, const QStringList &removed,
                          const QString &src, bool reconcileDisk)
{
/*
    Change the loaded datamodel -- insert new files, replace files rewritten in place,
    remove files that are gone -- and bring every consumer of it back in line.

    THE ORDER IS THE FIX. Each of the defects this pipeline replaced was a step run too
    early or not at all, by one of several callers that each did its own subset:

      1  Bump the instance BEFORE any row moves. MetaRead and ImageCache write to the
         model by ROW NUMBER, stamped with the instance they were started under; an
         insert or remove shifts rows under a write already in flight, which then lands
         on the wrong image. A new instance makes the model drop those writes. refreshViews
         -> MW::filterChange re-initializes the readers on the new instance.
      2  Mutate, through DataModel's batch calls only (insertFiles / removeFiles /
         refresh). They rebuild fPathRow once, after every row has its path.
      3  Load the new and changed rows' metadata and thumbnails SYNCHRONOUSLY, so each
         ends MetaLoaded with a current icon. The async MetaRead re-read does not
         reliably complete in this flow, and ImageCache only decodes MetaLoaded rows.
      4  refreshViews: proxy re-assert (in source order, now that metadata has landed and
         cannot kick the rows to the end again), ImageCache, selection, icons, video and
         the Develop panel.
      5  Filters LAST, once the rows carry their values -- saved, rebuilt and restored
         (restoreFiltersAfterFolderChange on finishedBuildFilters), so an active filter
         survives an insert or a delete.
      6  verifyIntegrity.

    See "DataModel On-the-Fly Insert and Delete" in notes/Documentation.txt.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::applyModelChange", src);
    if (added.isEmpty() && removed.isEmpty() && !reconcileDisk) return;

    // 1  no row-addressed write started before this may land after it
    dm->newInstance("applyModelChange: " + src);

    // 2  mutate
    const int rowsBefore = dm->rowCount();
    QStringList toLoad;
    if (!removed.isEmpty()) dm->removeFiles(removed);
    for (const QString &fPath : added) {
        if (fPath.isEmpty() || toLoad.contains(fPath)) continue;
        toLoad << fPath;
        const int dmRow = dm->rowFromPath(fPath);
        if (dmRow < 0) continue;                    // new: inserted below
        /* Rewritten in place (a re-run focus stack, a re-embellish): forget everything
           decoded from the old content. */
        dm->setData(dm->index(dmRow, G::MetadataStatusColumn), G::MetaNotAttempted);
        dm->setData(dm->index(dmRow, G::IconLoadedColumn), false);
        dm->setData(dm->index(dmRow, G::IsCachedColumn), false);
        dm->setData(dm->index(dmRow, G::IsCachingColumn), false);
        dm->setData(dm->index(dmRow, G::AttemptsColumn), 0);
        dm->setData(dm->index(dmRow, 0), QVariant(), Qt::DecorationRole);
        imageCache->removeCachedImage(fPath);
    }
    if (!added.isEmpty()) dm->insertFiles(added);
    if (reconcileDisk) toLoad << dm->refresh();
    toLoad.removeDuplicates();
    /*  A Refresh that found nothing on disk changed nothing: the views are still brought
        up to date below (as Refresh always did), but the filters are not rebuilt. */
    const bool changed = !toLoad.isEmpty() || !removed.isEmpty() ||
                         dm->rowCount() != rowsBefore;

    // 3  metadata + thumbnail, synchronously
    if (!toLoad.isEmpty() && !refreshThumb && metaRead)
        refreshThumb = new Thumb(dm, metaRead->getFrameDecoder());

    for (const QString &fPath : std::as_const(toLoad)) {
        const int dmRow = dm->rowFromPath(fPath);
        if (dmRow < 0) continue;
        if (dm->index(dmRow, G::MetadataStatusColumn).data().toInt() == G::MetaLoaded)
            continue;

        QFileInfo fileInfo(fPath);
        if (!metadata->loadImageMetadata(fileInfo, dmRow, dm->instance,
                                         true, true, false, true, src))
            continue;
        dm->addMetadataForItem(metadata->m, src);

        /* setIcon will not replace a live icon, so clear the stale decoration first,
           then load the new embedded thumb (metadata->m, just read, has its offset). */
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

    // 4  image counts, proxy, ImageCache, selection, views
    fsTree->updateCount();
    bookmarks->updateCount();
    updateLibraryTree();
    refreshViews(src);

    // 5  filters last, keeping the user's checks
    if (changed) {
        if (!restoreFiltersPending) {
            filters->save();
            restoreFiltersPending = true;
        }
        buildFilters->rebuild();
    }

    // 6
    dm->verifyIntegrity(src);
}

void MW::insertFiles(QStringList pathList)
{
/*
    Insert new image files -- or replace ones rewritten in place -- in the loaded
    datamodel: focus stack, embellish and export results. See applyModelChange.
*/
    if (G::isLogger) G::log("MW::insertFiles", "dm->instance = " + QString::number(dm->instance));

    if (pathList.isEmpty()) {
        QString msg = "No files to insert, fPaths is empty.";
        G::issue("Warning", msg, "MW::insertFiles");
        return;
    }
    applyModelChange(pathList, QStringList(), "MW::insertFiles");
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

    G::ScrollSignalGuard scrollGuard;   // deleting rows scrolls; that is not the user

    /* PROGRESS AND CANCEL. Trashing is one OS call per file (plus its sidecars), so a
       large selection runs for tens of seconds however lean the rest is. The dialog is
       window-modal, so nothing can change the folder or the selection mid-delete, and
       setValue pumps events once per FileOps chunk rather than once per file. A
       cancel stops between chunks; whatever was trashed by then still leaves the model
       below, so the model always matches the disk. */
    QElapsedTimer t;
    t.start();
    const int total = paths.count();
    QProgressDialog *progress = nullptr;
    if (total > 20) {
        progress = new QProgressDialog("Moving " + QString::number(total) +
                                       " images to the " + G::trash + "...",
                                       "Cancel", 0, total, this);
        progress->setWindowTitle("Delete Images");
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(500);
        progress->setAutoClose(false);
        progress->setAutoReset(false);
        progress->setStyleSheet(G::css);
        progress->setValue(0);
    }

    FileOps::TrashResult result = FileOps::trashFiles(paths, [&](int done) {
        if (!progress) return true;
        progress->setLabelText("Moving images to the " + G::trash + ": " +
                               QString::number(done) + " of " + QString::number(total));
        progress->setValue(done);
        return !progress->wasCanceled();
    });
    const qint64 trashMs = t.restart();

    if (progress) {
        progress->setLabelText("Updating the image list...");
        progress->setCancelButton(nullptr);
        progress->setValue(progress->value());      // repaint the new label
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    /* update datamodel, imagecache, image counts. A missing file is gone from disk
       either way (a drag-move out of IconView lands here after the target moved it), so
       its row goes too. */
    refreshAfterRemoval(result.trashed + result.missing);
    const qint64 refreshMs = t.elapsed();

    // TEMPORARY: bulk delete timing, until the 7,000 image case is measured in the app
    qDebug() << "MW::deleteFiles" << total << "requested" << result.trashed.size()
             << "trashed" << result.failed.size() << "failed"
             << "cancelled" << result.cancelled
             << "trash ms" << trashMs << "refresh ms" << refreshMs;

    // reset LAST, after every phase
    if (progress) {
        progress->close();
        progress->deleteLater();
    }

    if (result.cancelled || !result.failed.isEmpty()) {
        QString msg;
        if (result.cancelled)
            msg = "Cancelled: " + QString::number(result.trashed.size()) + " of " +
                  QString::number(total) + " images were moved to the " + G::trash + ".";
        else
            msg = QString::number(result.trashed.size()) + " images were moved to the " +
                  G::trash + ".";
        if (!result.failed.isEmpty())
            msg += "<br>" + QString::number(result.failed.size()) +
                   " could not be moved (locked or protected). See the Issues log.";
        G::popup->showPopup(msg, 4000);
    }

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

void MW::createFolderFromContext()
{
/*
    Context menu "Create folder in <folder>" in the Folders panel (FSTree). The parent is
    the folder under the right mouse click (mouseOverFolderPath, set in MW::eventFilter).
*/
    if (G::isLogger) G::log("MW::createFolderFromContext", mouseOverFolderPath);
    if (mouseOverFolderPath.isEmpty()) return;
    if (refuseCacheFolder(mouseOverFolderPath)) return;
    fsTree->createFolder(mouseOverFolderPath);
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

    if (refuseCacheFolder(dirToDelete)) return;

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
