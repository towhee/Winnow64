#include "Main/mainwindow.h"
#include "Utilities/versionkey.h"
#include "Metadata/versions.h"
#include "Utilities/rowtrail.h"
#include "Utilities/fileops.h"
#include "Cache/devpreviewcache.h"
#include "Cache/catalog.h"
#include "Cache/pathkey.h"
#include "Utilities/foldertree.h"
#include <QSet>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QMessageBox>

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
    QModelIndexList selection = RowSelection::Rows(dm->selectionModel);
    if (selection.isEmpty()) return;

    // flush unsaved per-image Develop edits so copied sidecars are current
    FileOps::flushPendingEdits();

    bool isSidecar = false;
    int n = selection.count();
    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;

    QSet<QString> seenFiles;
    for (int i = 0; i < n; ++i) {
        // add image path
        QString fPath = selection.at(i).data(G::SourcePathRole).toString();
        /* a version (virtual copy) is its source file: hand each file over once */
        if (seenFiles.contains(fPath)) continue;
        seenFiles.insert(fPath);
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
        folderAndFileSelectionChange(dm->currentKey, "pasteFiles");
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
    QModelIndexList selection = RowSelection::Rows(dm->selectionModel);
    // each FILE once: a version (virtual copy) is its source file
    QStringList list;
    for (const QModelIndex &idx : selection) {
        const QString p = idx.data(G::SourcePathRole).toString();
        if (!p.isEmpty() && !list.contains(p)) list << p;
    }
    const int n = list.size();
    QApplication::clipboard()->setText(list.join("\n"));

    QString nPaths;
    if (n == 1) nPaths = "1 path";
    else nPaths = QString::number(n) + " paths";
    QString msg = "Copied " + nPaths + " to the clipboard";
    G::popup->showPopup(msg, 1500);
}

void MW::renameSelectedFiles()
{
    QStringList selection;
    if (!dm->getSelectionOrPicks(selection)) return;
    /* getSelection can return true with an empty list (no picks, no selection);
    opening the dialog in that state crashes when updateExample touches
    selection.at(0) via the combobox's currentTextChanged signal. */
    if (selection.isEmpty()) {
        G::popup->showPopup("No images selected to rename.", 2000);
        return;
    }

    // Pre-check for Finder-locked files (macOS UF_IMMUTABLE). rename() fails
    // with EPERM on such files, so warn and bail before opening the dialog —
    // the user has to unlock in Finder first.
    /* The selection may span folders (Library / Catalog mode, recursive folders);
       RenameFileDlg renames folder by folder, so check each folder against only its
       own images. */
    QMap<QString, QStringList> pathsInFolder;
    for (const QString &path : std::as_const(selection))
        pathsInFolder[QFileInfo(path).path()] << path;
    QStringList lockedFiles;
    for (auto it = pathsInFolder.cbegin(); it != pathsInFolder.cend(); ++it)
        lockedFiles << RenameFileDlg::lockedFilesInSelection(it.key(), it.value());
    if (!lockedFiles.isEmpty()) {
        QMessageBox::warning(this, "File locked",
                             RenameFileDlg::lockedFilesMsg(lockedFiles));
        return;
    }

    /* A rename inside the 2s Develop debounce would otherwise let the pending write
       land afterwards and recreate a sidecar at the OLD name. */
    FileOps::flushPendingEdits();

    RenameFileDlg rf(this, selection, filenameTemplates,
                     dm, metadata, imageCache);
    rf.exec();

    // may have renamed current image
    titleFilePath = dm->currentSourcePath();
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

    QModelIndexList selection = RowSelection::Rows(dm->selectionModel);
    if (selection.isEmpty()) return;

    QList<QUrl> urls;
    QSet<QString> seenFiles;
    for (int i = 0; i < selection.count(); ++i) {
        QString fPath = selection.at(i).data(G::SourcePathRole).toString();
        /* a version (virtual copy) is its source file: hand each file over once */
        if (seenFiles.contains(fPath)) continue;
        seenFiles.insert(fPath);
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

    ExportDlg dlg(imageExporter, exportPresets, targets, dm->currentKey,
                  filenameTemplates, ExportDlg::Mode::Preview, this);
    const bool go = dlg.exec() == QDialog::Accepted;
    if (exportPresets) exportPresets->writeLast(dlg.settings());
    if (!go) return;
    runExport(dlg.targetPaths(), dlg.settings(),
        [this](const QString &fPath, ImageExporter::Done done) {
            previewPixelSource(fPath, done);
        });
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
    RowTrail::Tag _trailTag("MW::applyModelChange");
    if (G::isLogger || G::isFlowLogger) G::log("MW::applyModelChange", src);
    if (added.isEmpty() && removed.isEmpty() && !reconcileDisk) return;

    // 1  no row-addressed write started before this may land after it
    dm->newInstance("applyModelChange: " + src);

    // 2  mutate
    const int rowsBefore = dm->rowCount();
    QStringList toLoad;
    QStringList rewritten;
    if (!removed.isEmpty()) dm->removeFiles(removed);
    for (const QString &fPath : added) {
        if (fPath.isEmpty() || toLoad.contains(fPath)) continue;
        toLoad << fPath;
        const int dmRow = dm->rowFromKey(fPath);
        if (dmRow < 0) continue;                    // new: inserted below
        /* Rewritten in place (a re-run focus stack, a re-embellish): forget everything
           decoded from the old content. */
        dm->setData(dm->index(dmRow, G::MetadataStatusColumn), G::MetaNotAttempted);
        dm->setData(dm->index(dmRow, G::IconLoadedColumn), false);
        dm->setData(dm->index(dmRow, G::IsCachedColumn), false);
        dm->setData(dm->index(dmRow, G::IsCachingColumn), false);
        dm->setData(dm->index(dmRow, G::AttemptsColumn), 0);
        dm->setData(dm->index(dmRow, 0), QVariant(), Qt::DecorationRole);
        /*  addMetadataForItem APPENDS to the search text, so a re-read would double every
            field in it. Start it again from what addFileDataForRow put there. */
        dm->setData(dm->index(dmRow, G::SearchTextColumn),
                    fPath + dm->index(dmRow, G::ModifiedColumn).data().toString());
        rewritten << fPath;
    }
    // the decoded images are of the old content (Documentation 4.4.13)
    if (!rewritten.isEmpty()) imageCache->invalidate(rewritten, "MW::applyModelChange");
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
        const int dmRow = dm->rowFromKey(fPath);
        if (dmRow < 0) continue;
        if (dm->index(dmRow, G::MetadataStatusColumn).data().toInt() == G::MetaLoaded)
            continue;

        /*  A VERSION ROW has no file of its own: its values come from its master
            (DataModel::fillVersionRow) and its icon from its own preview, or the source
            file's thumbnail (Thumb::loadVersionThumb). See Utilities/versionkey.h. */
        if (VersionKey::isVersion(fPath)) {
            if (!dm->fillVersionRow(dmRow)) continue;
            if (refreshThumb) {
                QModelIndex iconIdx = dm->index(dmRow, 0);
                dm->setData(iconIdx, QVariant(), Qt::DecorationRole);
                QImage image;
                if (refreshThumb->loadVersionThumb(fPath, dmRow, image, dm->instance,
                                                   metadata, src)) {
                    QImage icon = image.scaled(G::maxIconSize, G::maxIconSize,
                                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    dm->setIcon(iconIdx, QPixmap::fromImage(icon), dm->instance, src);
                }
            }
            continue;
        }

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
            /*  The same set, less what left it: a check whose value went with the rows
                (every 5-star image deleted) stays checked on an empty item. */
            restoreKeepsMissing = true;
        }
        /*  The rebuild unchecks the tree until the restore lands; a re-filter in between
            (a second applyModelChange, as New Version makes) must keep the user's checks.
            Released by restoreFiltersAfterFolderChange. See SortFilter::holdPredicate. */
        dm->sf->holdPredicate(true, "MW::applyModelChange");
        // no blank panel while it rebuilds; released by restoreFiltersAfterFolderChange
        filters->holdPaint(true, "MW::applyModelChange");
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
                      "Press " + G::keyHtml("Esc") + " to continue.";
        G::popup->showPopup(msg, 0);
        return;
    }

    // QModelIndexList selection = dm->selectionModel->selectedRows();
    // if (selection.isEmpty()) return;
    QModelIndexList selection;
    if (G::mode == "Grid") {
        selection = RowSelection::Rows(gridView->selectionModel());
    } else if (G::mode == "Table") {
        selection = RowSelection::Rows(tableView->selectionModel());
    } else {
        selection = RowSelection::Rows(dm->selectionModel);
    }

    if (selection.isEmpty()) {
        G::popup->showPopup("No images selected to delete.", 1500);
        return;
    }

    /*  VERSIONS (virtual copies) ARE NOT FILES. A selected version is deleted as a RECORD
        in its master's sidecar (MW::deleteVersionRecords) -- never by trashing the image,
        which would take the master and every other version with it. A selected master
        is trashed and its versions go with it (they live in its sidecar), so a version
        whose master is also selected needs nothing of its own. */
    QStringList paths;
    QStringList versionsOnly;
    paths.reserve(selection.size());
    for (const QModelIndex &sfIdx : selection) {
        QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
        QString fPath = dmIdx.data(G::KeyRole).toString();
        if (fPath.isEmpty()) continue;
        if (VersionKey::isVersion(fPath)) versionsOnly << fPath;
        else paths << fPath;
    }
    paths.removeDuplicates();
    for (int i = versionsOnly.size() - 1; i >= 0; --i)
        if (paths.contains(VersionKey::sourceOf(versionsOnly.at(i)))) versionsOnly.removeAt(i);
    int versionsGoing = 0;
    for (const QString &p : std::as_const(paths)) versionsGoing += dm->versionKeysOf(p).size();

    if (paths.isEmpty()) {                      // only versions: no file is touched
        deleteVersionRecords(versionsOnly);
        return;
    }

    /*  Warning MessageBox. Always shown when any version is going, whether to the trash
        with its image or as a selected version's record: turning the warning off agreed
        to trashing images, not to losing versions. */
    if (deleteWarning || versionsGoing > 0 || !versionsOnly.isEmpty()) {
        QMessageBox msgBox(this);
        int msgBoxWidth = 300;
        msgBox.setWindowTitle("Delete Images");
        QString selectedImages = QString("%1 selected image%2")
                                     .arg(paths.size()).arg(paths.size() == 1 ? "" : "s");
        #ifdef Q_OS_WIN
        msgBox.setText("This operation will move " + selectedImages + " to the recycle bin.");
        #endif
        #ifndef Q_OS_WIN
        msgBox.setText("This operation will move " + selectedImages + " to the trash.");
        #endif
        QString info = "Do you want continue?";
        if (versionsGoing > 0)
            info = QString("%1 version%2 of these images will be deleted with them.\n\n")
                       .arg(versionsGoing).arg(versionsGoing == 1 ? "" : "s") + info;
        if (!versionsOnly.isEmpty())
            info = QString("%1 selected version%2 will be removed; their images stay.\n\n")
                       .arg(versionsOnly.size()).arg(versionsOnly.size() == 1 ? "" : "s")
                   + info;
        msgBox.setInformativeText(info);
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

    if (!versionsOnly.isEmpty()) deleteVersionRecords(versionsOnly, /*confirm*/false);
    deleteFiles(paths);
}

void MW::deleteRejectedFiles()
{
/*
    Select every rejected image in the current view (the filtered proxy) and run
    deleteImagesAction on it, so the confirmation dialog, version handling and
    trash/recycle bin path are exactly those of Delete. Rejected images hidden by a
    filter are not selectable and are left alone.
*/
    if (G::isLogger) G::log("MW::deleteRejectedFiles");

    if (!G::allMetadataAttempted) {
        deleteSelectedFiles();                  // shows the "wait until loaded" popup
        return;
    }

    /* A frozen selection (Compare) refuses selectRows, so deleteImagesAction would
       trash the frozen images instead of the rejects. */
    if (sel->isFrozen()) {
        G::popup->showPopup("Unfreeze the selection before deleting rejected images.", 2000);
        return;
    }

    QList<int> rejectedRows;
    const int n = dm->sf->rowCount();
    for (int sfRow = 0; sfRow < n; ++sfRow) {
        if (dm->sf->index(sfRow, G::PickColumn).data(Qt::EditRole).toString() == "Rejected")
            rejectedRows.append(sfRow);
    }

    if (rejectedRows.isEmpty()) {
        G::popup->showPopup("No rejected images to delete.", 1500);
        return;
    }

    sel->selectRows(rejectedRows, false);
    deleteImagesAction->trigger();
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
                      "Press " + G::keyHtml("Esc") + " to continue.";
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

    /* Update selection. When every filtered item is deleted the filters stay and
       the proxy is empty, so nothing is selected; otherwise the prior current row may
       be gone (dm->currentSfRow -1), so fall back to the first row. */
    int sfRow = dm->currentSfRow;
    if (sfRow < 0 || sfRow >= dm->sf->rowCount()) sfRow = 0;
    if (dm->sf->rowCount() > 0) sel->select(sfRow);
}

namespace {
/*  Where src lands in folder, and every file there that the move would replace: the
    image itself and the destination of each of its sidecars. A stray IMG_1.xmp with no
    image beside it is still a clash -- moveFile would overwrite it silently. */
QStringList moveClashes(const QString &src, const QString &dst)
{
    QStringList clashes;
    if (QFileInfo::exists(dst)) clashes << dst;
    for (const QString &c : FileOps::companions(src)) {
        const QString cDst = FileOps::companionDest(c, src, dst);
        if (QFileInfo::exists(cDst)) clashes << cDst;
    }
    return clashes;
}
}  // namespace

void MW::moveImagesToFolder(const QStringList &paths, const QStringList &rowKeys,
                            const QString &folder, bool copy)
{
/*
    Thumbnails dropped on a Folders row of the Filters panel in Library scope (see
    Filters::dropEvent). Each image moves with its sidecars through FileOps::moveFile,
    which also carries the caches, the catalog row, collections, picks and Develop
    history to the new path.

    ASK FIRST. A dense folder tree makes a drop on the wrong row easy, so the move is
    confirmed, and any name already taken in the destination is named in the same
    dialog with Rename (name_1), Overwrite (the existing files go to the trash) or
    Cancel.

    A VERSION IS NOT A FILE. It is a second recipe in its master's sidecar, so dragging
    one moves the master file and every version with it. The drag's row keys say which
    versions came without their master, and the dialog names them (versionNote).

    COPY (Opt/Alt drop, copy = true) is the same routine with FileOps::copyFile: the
    same confirmation and clash choices, the originals stay, and the copies come in as
    new rows. FileOps::onCopied reports each copy to the catalog (onCreated), and
    catalogCreatedFilesThenRefreshLibrary indexes them now and refreshes the Library
    once they are committed. A version copies with its master, as it moves with it.

    THE MODEL follows through applyModelChange -- the old rows out, the new ones read
    in, the filters saved, rebuilt and restored -- and the Library re-query and LibTree
    counts through refreshLibraryAfterCatalogChange, whose delta then finds nothing
    left to do.
*/
    if (G::isLogger) G::log("MW::moveImagesToFolder", folder);
    const QString verb = copy ? "Copy" : "Move";              // button, title
    const QString verbing = copy ? "Copying" : "Moving";
    const QString verbed = copy ? "Copied" : "Moved";
    if (G::scope != G::Scope::Catalog || folder.isEmpty()) return;
    if (G::isModifyingDatamodel || G::isLoadRunning) {
        G::popup->showPopup("Please wait until the Library has finished loading "
                            "before " + verbing.toLower() + " images.", 3000);
        return;
    }
    if (!QFileInfo(folder).isDir()) {
        G::popup->showPopup(QDir::toNativeSeparators(folder) + " is not available.", 3000);
        return;
    }

    const QDir dir(folder);
    QStringList srcs;
    for (const QString &p : paths) {
        if (p.isEmpty() || VersionKey::isVersion(p) || srcs.contains(p)) continue;
        const QFileInfo info(p);
        if (!info.exists()) continue;
        // a drop on the image's own folder moves (or copies) nothing
        if (QDir(info.absolutePath()) == dir) continue;
        srcs << p;
    }
    if (srcs.isEmpty()) {
        G::popup->showPopup("Already in " + dir.dirName() + ".", 2000);
        return;
    }

    QStringList clashing;                   // the srcs whose destination is taken
    for (const QString &src : std::as_const(srcs))
        if (!moveClashes(src, dir.filePath(QFileInfo(src).fileName())).isEmpty())
            clashing << src;

    /* Versions dragged without their master row, by master: the move takes the master
       and all of its versions, which the user did not visibly ask for. */
    QMap<QString, QStringList> lonelyVersions;
    for (const QString &k : rowKeys) {
        if (!VersionKey::isVersion(k)) continue;
        const QString master = VersionKey::sourceOf(k);
        if (!srcs.contains(master) || rowKeys.contains(master)) continue;
        const int row = dm->rowFromKey(k);
        QString name = row < 0 ? QString()
                               : dm->index(row, G::PathColumn)
                                     .data(G::VersionNameRole).toString();
        if (name.isEmpty())                         // no row: the sidecar knows
            name = Versions::readVersion(master, VersionKey::idOf(k)).displayName();
        lonelyVersions[master] << name;
    }
    QString versionNote;
    if (!lonelyVersions.isEmpty()) {
        const QStringList allVersionKeys = dm->versionRowKeys();
        const int show = 5;
        int shown = 0;
        for (auto it = lonelyVersions.cbegin(); it != lonelyVersions.cend(); ++it) {
            if (shown++ == show) {
                versionNote += "<br>&nbsp;&nbsp;...and " +
                               QString::number(lonelyVersions.size() - show) + " more";
                break;
            }
            int total = 0;
            for (const QString &k : allVersionKeys)
                if (VersionKey::sourceOf(k) == it.key()) ++total;
            const QString file = QFileInfo(it.key()).fileName().toHtmlEscaped();
            versionNote += "<br>&nbsp;&nbsp;" + file + " " +
                           it.value().join(", ").toHtmlEscaped() + " &rarr; " +
                           verb.toLower() + "s " + file + (total == 1 ? " and its version"
                                              : " and all " + QString::number(total) +
                                                " of its versions");
        }
        const bool one = lonelyVersions.size() == 1 && lonelyVersions.first().size() == 1;
        versionNote = QString(one ? "This is a version" : "These are versions") +
                      ", not separate files. Versions live in their image's sidecar, so "
                      + verbing.toLower() + " one " + verb.toLower() + "s the image and every "
                      "version of it:" + versionNote;
    }

    // confirm
    const int n = srcs.size();
    const QString images = n == 1 ? "1 image" : QString::number(n) + " images";
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(verb + " Images");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setIcon(clashing.isEmpty() ? QMessageBox::Question : QMessageBox::Warning);
    msgBox.setText(verb + " " + images + " and " + (n == 1 ? "its" : "their") +
                   " sidecars to <b>" + dir.dirName().toHtmlEscaped() + "</b>?<br>" +
                   G::secondaryHtml(QDir::toNativeSeparators(folder).toHtmlEscaped()));
    QPushButton *moveBtn = nullptr;
    QPushButton *renameBtn = nullptr;
    QPushButton *overwriteBtn = nullptr;
    if (clashing.isEmpty()) {
        moveBtn = msgBox.addButton(verb, QMessageBox::AcceptRole);
        msgBox.setDefaultButton(moveBtn);
        if (!versionNote.isEmpty()) msgBox.setInformativeText(versionNote);
    }
    else {
        const int show = 10;
        QString names;
        for (int i = 0; i < qMin(show, clashing.size()); ++i)
            names += "<br>&nbsp;&nbsp;" + QFileInfo(clashing.at(i)).fileName().toHtmlEscaped();
        if (clashing.size() > show)
            names += "<br>&nbsp;&nbsp;...and " + QString::number(clashing.size() - show) +
                     " more";
        msgBox.setInformativeText(
            QString::number(clashing.size()) + (clashing.size() == 1 ? " is" : " are") +
            " already there (the image or its sidecar):" + names +
            "<p><b>Rename</b> " + verb.toLower() + "s " + (clashing.size() == 1 ? "it" : "them") +
            " under a new name (name_1). <b>Overwrite</b> sends the existing files to the " +
            G::trash + " first." +
            (versionNote.isEmpty() ? QString() : "<p>" + versionNote));
        renameBtn = msgBox.addButton("Rename", QMessageBox::AcceptRole);
        overwriteBtn = msgBox.addButton("Overwrite", QMessageBox::DestructiveRole);
        msgBox.setDefaultButton(renameBtn);
    }
    QPushButton *cancelBtn = msgBox.addButton(QMessageBox::Cancel);
    msgBox.setEscapeButton(cancelBtn);
    msgBox.setStyleSheet(G::css);
    msgBox.exec();
    resetFocus();
    QAbstractButton *clicked = msgBox.clickedButton();
    if (!clicked || clicked == cancelBtn) return;
    const bool overwrite = clicked == overwriteBtn;

    G::ScrollSignalGuard scrollGuard;       // removing rows scrolls; that is not the user

    QProgressDialog *progress = nullptr;
    if (n > 20) {
        progress = new QProgressDialog(verbing + " " + images + "...", "Cancel", 0, n, this);
        progress->setWindowTitle(verb + " Images");
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(500);
        progress->setAutoClose(false);
        progress->setAutoReset(false);
        progress->setStyleSheet(G::css);
        progress->setValue(0);
    }

    QStringList moved, added, removed, failed;
    bool cancelled = false;
    for (int i = 0; i < n; ++i) {
        if (progress) {
            progress->setValue(i);
            if (progress->wasCanceled()) { cancelled = true; break; }
        }
        const QString &src = srcs.at(i);
        const QFileInfo info(src);
        QString dst = dir.filePath(info.fileName());
        const QStringList clashes = moveClashes(src, dst);
        if (!clashes.isEmpty()) {
            if (overwrite) {
                /* The existing image goes with its own sidecars and caches; a stray
                   sidecar with no image is trashed by itself. Either way recoverable. */
                if (QFileInfo::exists(dst)) {
                    if (!FileOps::trashFile(dst)) { failed << src; continue; }
                    removed << dst;         // it may be a loaded Library row
                }
                for (const QString &c : moveClashes(src, dst))
                    QFile::moveToTrash(c);
                if (!moveClashes(src, dst).isEmpty()) { failed << src; continue; }
            }
            else {
                // rename: the first name_N free for the image and every sidecar
                const QString base = info.completeBaseName();
                const QString suffix = info.suffix().isEmpty() ? "" : "." + info.suffix();
                for (int k = 1; ; ++k) {
                    dst = dir.filePath(base + "_" + QString::number(k) + suffix);
                    if (moveClashes(src, dst).isEmpty()) break;
                }
            }
        }
        if (copy ? FileOps::copyFile(src, dst) : FileOps::moveFile(src, dst)) {
            moved << src;
            added << dst;
        }
        else failed << src;
    }

    if (progress) {
        progress->setLabelText("Updating the image list...");
        progress->setCancelButton(nullptr);
        progress->setValue(n);
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    if (!moved.isEmpty() || !removed.isEmpty()) {
        /* The rows come back under new keys, so what was keyed on the old path must
           follow, as DataModel::rekeyVersions does for a rename: an open version group
           would otherwise return closed (its versions hidden behind a "1 >" badge), and
           the current image -- a master or one of its versions -- would be lost. */
        for (int i = 0; !copy && i < moved.size(); ++i) {
            const QString &from = moved.at(i);
            const QString &to = added.at(i);
            if (dm->sf->expandedVersions().contains(from)) {
                dm->sf->setVersionsExpanded(from, false);
                dm->sf->setVersionsExpanded(to, true);
            }
            if (VersionKey::sourceOf(dm->currentKey) == from)
                dm->currentKey = VersionKey::isVersion(dm->currentKey)
                    ? VersionKey::make(to, VersionKey::idOf(dm->currentKey)) : to;
        }
        // a copy leaves its original's row where it is
        applyModelChange(added, copy ? removed : removed + moved, "MW::moveImagesToFolder");
        // a moved master's versions were dropped with its old row; bring them back
        reconcileVersionRows("MW::moveImagesToFolder");
        if (copy) catalogCreatedFilesThenRefreshLibrary();   // the copies are new files
        else refreshLibraryAfterCatalogChange();
    }

    // reset LAST, after every phase
    if (progress) {
        progress->close();
        progress->deleteLater();
    }

    const QString count = moved.size() == n
                              ? images : QString::number(moved.size()) + " of " + images;
    QString msg = (cancelled ? "Cancelled. " + verbed + " " : verbed + " ") + count + " to " +
                  dir.dirName().toHtmlEscaped() + ".";
    if (!failed.isEmpty())
        msg += "<br>" + QString::number(failed.size()) +
               " could not be " + verbed.toLower() + ". See the Issues log.";
    G::popup->showPopup(msg, failed.isEmpty() ? 2500 : 5000);

    /* The moved rows usually leave the filtered set (the source folder is what is
       checked), so the prior current row may be gone. */
    int sfRow = dm->currentSfRow;
    if (sfRow < 0 || sfRow >= dm->sf->rowCount()) sfRow = 0;
    if (dm->sf->rowCount() > 0) sel->select(sfRow);
}

namespace {
/*  Where path is once folder from has become to: to itself, or to plus the rest of a
    path beneath from. Empty when path is neither. Compared by cache key, so a spelling
    the filesystem treats as the same folder still matches. */
QString relocatedPath(const QString &path, const QString &from, const QString &to)
{
    const QString key = cachePathKey(path);
    const QString fromKey = cachePathKey(from);
    if (key.isEmpty() || fromKey.isEmpty()) return QString();
    if (key == fromKey) return to;
    if (key.startsWith(fromKey + "/")) return to + path.mid(from.size());
    return QString();
}

/*  The checks every Library folder operation shares. False, with a popup saying why,
    when the folder cannot be touched now. The Filters menu greys these same cases with
    the reason, so a popup here means the state changed while the menu was open. */
bool libraryFolderOpAllowed(const QString &folder, const QString &what)
{
    if (G::scope != G::Scope::Catalog || folder.isEmpty()) return false;
    if (G::isModifyingDatamodel || G::isLoadRunning) {
        G::popup->showPopup("Please wait until the Library has finished loading "
                            "before you " + what + " a folder.", 3000);
        return false;
    }
    if (!QFileInfo(folder).isDir()) {
        G::popup->showPopup(QDir::toNativeSeparators(folder) + " is not available.", 3000);
        return false;
    }
    if (refuseCacheFolder(folder)) return false;
    return true;
}
}  // namespace

void MW::renameLibraryFolder(const QString &folder)
{
/*
    "Rename folder..." on a Folders row in the Library. The new name is a NAME, not a
    path: the folder stays where it is. A name only the case of which differs is a
    rename too (relocateLibraryFolder takes it through a temporary name, which a
    case-insensitive volume needs).
*/
    if (G::isLogger) G::log("MW::renameLibraryFolder", folder);
    if (!libraryFolderOpAllowed(folder, "rename")) return;
    const QFileInfo info(folder);
    const QString oldName = info.fileName();

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Folder"), tr("New name for the folder \"%1\":").arg(oldName),
        QLineEdit::Normal, oldName, &ok).trimmed();
    resetFocus();
    if (!ok || name.isEmpty() || name == oldName) return;

    /* The characters no platform allows in a name, and the ones Windows adds. A name is
       one component, so a separator would silently make it a move. */
    QString bad = "/\\";
#ifdef Q_OS_WIN
    bad += ":*?\"<>|";
#endif
    for (const QChar c : bad) {
        if (name.contains(c)) {
            G::popup->showPopup(tr("A folder name cannot contain %1").arg(c), 3000);
            return;
        }
    }
    if (name == "." || name == "..") return;

    relocateLibraryFolder(folder, info.dir().filePath(name), "Rename");
}

void MW::moveLibraryFolder(const QString &folder)
{
/*
    "Move folder..." on a Folders row in the Library: the user picks the folder it is to
    go INTO, starting at the one that holds it now. The pick is the confirmation -- a
    folder dialog cannot be dropped on the wrong row the way a drag can.
*/
    if (G::isLogger) G::log("MW::moveLibraryFolder", folder);
    if (!libraryFolderOpAllowed(folder, "move")) return;
    const QFileInfo info(folder);
    const QString name = info.fileName();

    QString dest = QFileDialog::getExistingDirectory(
        this, tr("Move \"%1\" into").arg(name), info.absolutePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    resetFocus();
    if (dest.isEmpty()) return;
    dest = FolderTree::normalize(dest);

    if (cachePathKey(dest) == cachePathKey(info.absolutePath())) {
        G::popup->showPopup(tr("\"%1\" is already there.").arg(name), 2500);
        return;
    }
    if (!relocatedPath(dest, folder, folder).isEmpty()) {
        G::popup->showPopup(tr("A folder cannot be moved into itself."), 3000);
        return;
    }
    relocateLibraryFolder(folder, QDir(dest).filePath(name), "Move");
}

void MW::trashLibraryFolder(const QString &folder)
{
/*
    "Move folder to trash" on a Folders row in the Library: the same as the Folders
    panel's own, through deleteFolderPath -- the warning, the trash, the catalog rows and
    caches forgotten (FileOps::onFolderDeleted), the loaded rows removed and the
    Folders filter rebuilt without it, and the Library refreshed.
*/
    if (G::isLogger) G::log("MW::trashLibraryFolder", folder);
    if (!libraryFolderOpAllowed(folder, "delete")) return;
    deleteFolderPath(folder);
}

void MW::relocateLibraryFolder(const QString &from, const QString &to, const QString &verb)
{
/*
    Rename or move a folder in the Library, and bring everything that knows it by path
    along. See "Renaming, Moving and Deleting Folders in the Library" in
    notes/Documentation.md.

    REFUSED, NOT ATTEMPTED, when it cannot work cleanly: the destination already exists
    (a merge is a different operation), or it is on another drive -- a rename cannot
    cross volumes, and a copy-then-delete of a whole folder tree is too much to do
    behind a menu item without a way to watch or stop it.

    ONE RENAME ON DISK, THEN ONE PREFIX REWRITE PER STORE (FileOps::onFolderMoved), so
    a folder of thousands of images is not thousands of moves.

    THE SCOPE TABLE FOLLOWS. A scope row naming the folder, or a folder beneath it, is
    the user's statement about THAT folder, so it moves with it. What is left is the
    case of a folder moved OUT of every included folder: the table is definitive, so its
    images leave the catalog (and with it the Library), and the result says so.

    THE MODEL follows as moveImagesToFolder's does: the old rows out and the new ones in
    through applyModelChange, versions brought back by reconcileVersionRows, and the
    open version groups and current image re-keyed first so they survive it.
*/
    if (G::isLogger) G::log("MW::relocateLibraryFolder", from + " -> " + to);
    const QString name = QFileInfo(from).fileName();
    const QString newName = QFileInfo(to).fileName();
    const QString toParent = QFileInfo(to).absolutePath();
    const bool caseOnly = from != to && cachePathKey(from) == cachePathKey(to);

    if (!caseOnly && QFileInfo::exists(to)) {
        G::popup->showPopup(tr("There is already a folder called \"%1\" in %2.")
                                .arg(newName, QDir::toNativeSeparators(toParent)), 4000);
        return;
    }
    if (refuseCacheFolder(to)) return;
    if (!caseOnly) {
        const QStorageInfo a(from);
        const QStorageInfo b(toParent);
        if (a.isValid() && b.isValid() && a.rootPath() != b.rootPath()) {
            G::popup->showPopup(tr("\"%1\" is on a different drive. A folder can only be "
                                   "moved to another folder on the same drive.")
                                    .arg(QDir::toNativeSeparators(toParent)), 4000);
            return;
        }
    }

    // the sidecars on disk must be current before their folder moves
    FileOps::flushPendingEdits();

    /* The loaded rows that are leaving: every MASTER from under the folder. Removing a
       master takes its version rows with it; reconcileVersionRows brings them back under
       the new path. */
    QStringList oldMasters, newMasters;
    for (int r = 0; r < dm->rowCount(); ++r) {
        const QString key = dm->index(r, G::PathColumn).data(G::KeyRole).toString();
        if (key.isEmpty() || VersionKey::isVersion(key)) continue;
        const QString moved = relocatedPath(key, from, to);
        if (moved.isEmpty()) continue;
        oldMasters << key;
        newMasters << moved;
    }

    /* ON DISK. A case-only rename goes through a temporary name: on a case-insensitive
       volume the destination "exists" (it is the same folder), and a rename onto it is
       refused or does nothing. */
    bool ok = false;
    if (caseOnly) {
        const QString tmp = from + ".winnow-rename";
        if (!QFileInfo::exists(tmp) && QDir().rename(from, tmp)) {
            ok = QDir().rename(tmp, to);
            if (!ok) QDir().rename(tmp, from);
        }
    }
    else ok = QDir().rename(from, to);
    if (!ok) {
        const QString msg = tr("Could not %1 \"%2\". A file in it may be open in another "
                               "application, or the folder may be protected.")
                                .arg(verb.toLower(), name);
        G::issue("Warning", msg, "MW::relocateLibraryFolder", -1, from);
        G::popup->showPopup(msg, 4000);
        return;
    }

    // everything keyed on the old paths
    FileOps::onFolderMoved(from, to);

    // the scope table rows that name the folder or a folder under it
    bool scopeChanged = false;
    for (CatalogScopeEntry &e : catalogScope) {
        const QString moved = relocatedPath(e.path, from, to);
        if (moved.isEmpty()) continue;
        e.path = catalogScopeNormalize(moved);
        scopeChanged = true;
    }
    if (scopeChanged) {
        if (catalogRootsDlg) catalogRootsDlg->setScope(catalogScope);
        if (filters) {
            QStringList anchors;
            for (const CatalogScopeEntry &e : catalogScopeEffective(true)) anchors << e.path;
            filters->setFolderAnchors(anchors);
        }
    }

    // bookmarks on the folder or beneath it
    if (bookmarks) {
        bool marked = false;
        const QSet<QString> paths = bookmarks->bookmarkPaths;
        for (const QString &p : paths) {
            const QString moved = relocatedPath(p, from, to);
            if (moved.isEmpty()) continue;
            bookmarks->bookmarkPaths.remove(p);
            bookmarks->bookmarkPaths.insert(moved);
            marked = true;
        }
        if (marked) bookmarks->reloadBookmarks();
    }

    /* MOVED OUT OF THE LIBRARY: catalogued folders now under to that no include row
       admits leave the catalog, as a reconcile would remove them. */
    QStringList leaving;
    int leavingImages = 0;
    const QMap<QString, int> counts = Catalog::instance().folderCounts();
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (relocatedPath(it.key(), to, to).isEmpty()) continue;
        if (isCatalogScopeFolder(it.key())) continue;
        leaving << it.key();
        leavingImages += it.value();
    }
    if (!leaving.isEmpty()) Catalog::instance().forgetFolders(leaving);

    // the loaded rows
    if (!oldMasters.isEmpty()) {
        QProgressDialog *progress = nullptr;
        if (oldMasters.size() > 50) {
            progress = new QProgressDialog(tr("Updating the Library..."), QString(),
                                           0, 0, this);
            progress->setWindowTitle(verb + " Folder");
            progress->setWindowModality(Qt::WindowModal);
            progress->setMinimumDuration(0);
            progress->setStyleSheet(G::css);
            progress->show();
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        G::ScrollSignalGuard scrollGuard;   // removing rows scrolls; that is not the user
        QStringList added;
        for (int i = 0; i < oldMasters.size(); ++i) {
            const QString &o = oldMasters.at(i);
            const QString &n = newMasters.at(i);
            if (dm->sf->expandedVersions().contains(o)) {
                dm->sf->setVersionsExpanded(o, false);
                dm->sf->setVersionsExpanded(n, true);
            }
            if (VersionKey::sourceOf(dm->currentKey) == o)
                dm->currentKey = VersionKey::isVersion(dm->currentKey)
                    ? VersionKey::make(n, VersionKey::idOf(dm->currentKey)) : n;
            // a folder that left the Library comes back out of the model, not into it
            if (isCatalogScopeFolder(QFileInfo(n).absolutePath())) added << n;
        }
        /* A Folders check on the folder follows it, or goes with it when it left the
           Library; restore would otherwise keep it on an empty item under the old path. */
        if (filters)
            filters->relocateFolderChecks(from, isCatalogScopeFolder(to) ? to : QString());
        applyModelChange(added, oldMasters, "MW::relocateLibraryFolder");
        reconcileVersionRows("MW::relocateLibraryFolder");

        // reset LAST
        if (progress) {
            progress->close();
            progress->deleteLater();
        }
    }
    refreshLibraryAfterCatalogChange();

    QString msg = verb == "Rename"
        ? tr("Renamed \"%1\" to \"%2\".").arg(name.toHtmlEscaped(), newName.toHtmlEscaped())
        : tr("Moved \"%1\" to %2.").arg(name.toHtmlEscaped(),
              QDir::toNativeSeparators(toParent).toHtmlEscaped());
    if (leavingImages > 0)
        msg += "<br>" + tr("It is outside the Library's folders, so its %n image(s) "
                           "left the Library.", "", leavingImages);
    G::popup->showPopup(msg, leavingImages > 0 ? 5000 : 2500);

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
        /* Folders scope only: in the Library the loaded set spans many folders, so
           folderList.at(0) is an arbitrary one (updateLibraryOnlyActions disables it). */
        if (G::scope == G::Scope::Catalog || dm->folderList.isEmpty()) return;
        dirToDelete = dm->folderList.at(0);
    }
    else if (senderObject == "deleteFSTreeFolder") {
        dirToDelete = mouseOverFolderPath;
    }
    deleteFolderPath(dirToDelete);
}

void MW::deleteFolderPath(const QString &dirToDelete)
{
    if (G::isLogger) G::log("MW::deleteFolderPath", dirToDelete);
    if (dirToDelete.isEmpty()) return;
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
        #ifndef Q_OS_WIN
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

    /*  IN THE LIBRARY THE LOADED SET IS THE CATALOG'S, NOT THIS FOLDER'S, so trashing a
        folder takes its rows out of it -- it does not stop and clear the whole model, as
        it must when the folder is the one Folders scope is showing. The rows go through
        applyModelChange directly rather than waiting for the re-query's splice
        (refreshLibraryAfterCatalogChange), which does not run for every result (over
        kMaxSpliceDelta, an empty Library). Every MASTER at or under the folder: removing
        a master takes its version rows with it. */
    const bool library = G::scope == G::Scope::Catalog;
    QStringList goneMasters;
    if (library) {
        for (int r = 0; r < dm->rowCount(); ++r) {
            const QString key = dm->index(r, G::PathColumn).data(G::KeyRole).toString();
            if (key.isEmpty() || VersionKey::isVersion(key)) continue;
            if (!relocatedPath(key, dirToDelete, dirToDelete).isEmpty()) goneMasters << key;
        }
    }
    else if (dm->folderList.contains(dirToDelete)) {
        stop("deleteFolder");
        // reset();
        setCentralMessage(dirToDelete + "\n has been sent to the " + G::trash);
    }

    // okay to delete
    if (QFile(dirToDelete).moveToTrash()) {
        const int forgotten = FileOps::onFolderDeleted(dirToDelete);
        if (!goneMasters.isEmpty()) {
            /* A Folders check on the trashed folder goes with it; restore would
               otherwise keep it, checked, on an empty item (Filters::restore). */
            if (filters) filters->relocateFolderChecks(dirToDelete, QString());
            G::ScrollSignalGuard scrollGuard;   // removing rows scrolls; not the user
            applyModelChange(QStringList(), goneMasters, "MW::deleteFolderPath");
        }
        if (forgotten > 0 || !goneMasters.isEmpty()) refreshLibraryAfterCatalogChange();
        if (!goneMasters.isEmpty()) {
            int sfRow = dm->currentSfRow;
            if (sfRow < 0 || sfRow >= dm->sf->rowCount()) sfRow = 0;
            if (dm->sf->rowCount() > 0) sel->select(sfRow);
        }
    }

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
    #ifndef Q_OS_WIN
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
    if (FileOps::onFolderDeleted(dcimPath) > 0) refreshLibraryAfterCatalogChange();

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
