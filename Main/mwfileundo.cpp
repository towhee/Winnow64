#include "Main/mainwindow.h"
#include "Undo/undomanager.h"
#include "Utilities/fileops.h"
#include "Utilities/versionkey.h"
#include "Metadata/versions.h"
#include "Datamodel/collectionstore.h"
#include "Datamodel/savedpicks.h"
#include "Datamodel/duplicateok.h"
#include "Develop/History/historystore.h"
#include "Undo/fileundo.h"
#include <memory>

/*
    EDIT > UNDO OF FILE OPERATIONS (Undo plan P3). See "Undo" in notes/Documentation.md.

    THE TRASH IS THE UNDO BUFFER. A delete moves the image and its sidecars to the
    trash (FileOps::trashFiles), which reports where each file went; undo moves them
    back (FileOps::restoreFromTrash). The sidecar carries the image's own data -- rating,
    keywords, recipe, versions -- so what has to be kept here is only what the delete
    PURGED from Winnow's stores (FileOps::onDeleted): collection memberships, saved
    picks, Duplicate OK and Develop History, plus the model-only pick state (a reject).
    The rebuildable caches (thumbnails, loupe previews, catalog rows) are rebuilt.

    The same pair -- trash with a record, restore from it -- undoes a paste and a copy
    (Undo trashes the new files, Redo brings them back) and redoes a delete.

    REFUSED, NOT HALF DONE. A file operation cannot run while a load is changing the
    model, and its files must be where it left them. A CheckedCommand returns false
    then, the stack stays where it was (UndoCommand::refuse), and a popup says why.
*/

namespace {

bool fileOpsBusy()
{
    if (!G::isModifyingDatamodel && !G::isLoadRunning) return false;
    G::popup->showPopup("Please wait until the images have finished loading, then "
                        "undo again.", 2500);
    return true;
}

/* rename from -> to; a case-only change goes through a temporary name, which a
   case-insensitive volume needs. Refuses a destination that is another file. */
bool renameExact(const QString &from, const QString &to)
{
    if (from == to) return true;
    if (from.compare(to, Qt::CaseInsensitive) == 0) {
        const QString tmp = to + ".winnow-rename";
        if (QFileInfo::exists(tmp) || !QFile::rename(from, tmp)) return false;
        if (QFile::rename(tmp, to)) return true;
        QFile::rename(tmp, from);
        return false;
    }
    if (QFileInfo::exists(to)) return false;
    return QFile::rename(from, to);
}

}   // namespace

QList<TrashedImage> MW::keepForTrashUndo(const QStringList &paths)
{
/*
    What the trash will not keep: each image's rows in the stores FileOps::onDeleted
    purges, for the image and each of its versions. Called just before the trash.
*/
    if (G::isLogger) G::log("MW::keepForTrashUndo", QString::number(paths.size()));
    FileOps::flushPendingEdits();
    CollectionStore &store = CollectionStore::instance();
    const bool collections = store.isAvailable();
    QList<TrashedImage> out;
    for (const QString &path : paths) {
        if (path.isEmpty() || VersionKey::isVersion(path)) continue;
        TrashedImage t;
        t.path = path;
        QStringList keys{path};
        // the model knows a loaded image's versions; otherwise its sidecar does
        if (dm && dm->rowFromKey(path) >= 0) keys << dm->versionKeysOf(path);
        else for (const ImageVersion &v : Versions::read(path).versions)
            keys << VersionKey::make(path, v.id);
        for (const QString &key : keys) {
            if (collections) {
                const QVector<qint64> ids = store.collectionsOf(key, /*treeOrder*/ false);
                if (!ids.isEmpty()) t.collections.insert(key, ids);
            }
            if (SavedPicks::Contains(key)) t.savedPicks << key;
            const int row = dm ? dm->rowFromKey(key) : -1;
            if (row >= 0) {
                const QVariant pick = dm->index(row, G::PickColumn).data(Qt::EditRole);
                if (!pick.toString().isEmpty()) t.pickState << qMakePair(key, pick);
            }
            TrashedImage::History h;
            h.key = key;
            if (HistoryStore::load(key, h.entries, h.pos, h.stamp)) t.history << h;
        }
        t.duplicateOk = DuplicateOk::IsOk(path);
        out << t;
    }
    return out;
}

bool MW::trashForUndo(QList<TrashedImage> &items)
{
/*
    Trash the images items names (the ones still on disk) and keep, in items, what
    restoreTrashed needs to bring them back. A Delete's Redo, and a paste's or copy's
    Undo. False, having said why, when nothing was trashed.
*/
    if (G::isLogger) G::log("MW::trashForUndo", QString::number(items.size()));
    if (fileOpsBusy()) return false;
    QStringList paths;
    for (const TrashedImage &t : std::as_const(items))
        if (QFileInfo::exists(t.path)) paths << t.path;
    if (paths.isEmpty()) {
        G::popup->showPopup("The files are no longer there.", 2500);
        return false;
    }
    G::ScrollSignalGuard scrollGuard;   // removing rows scrolls; that is not the user
    QList<TrashedImage> kept = keepForTrashUndo(paths);
    const FileOps::TrashResult r = FileOps::trashFiles(paths);

    QStringList rows;                   // only what the model holds leaves it
    for (const QString &p : r.trashed) if (dm->rowFromKey(p) >= 0) rows << p;
    if (!rows.isEmpty()) refreshAfterRemoval(rows);
    if (G::scope == G::Scope::Catalog) refreshLibraryAfterCatalogChange();
    for (const QString &p : r.trashed) fsTree->updateAFolderCount(QFileInfo(p).absolutePath());

    items.clear();
    for (TrashedImage &t : kept) {
        const auto it = r.inTrash.constFind(t.path);
        if (it == r.inTrash.cend()) continue;
        t.files = it.value();
        items << t;
    }
    int sfRow = dm->currentSfRow;
    if (sfRow < 0 || sfRow >= dm->sf->rowCount()) sfRow = 0;
    if (dm->sf->rowCount() > 0) sel->select(sfRow);
    return !items.isEmpty();
}

bool MW::restoreTrashed(const QList<TrashedImage> &items)
{
/*
    Edit > Undo of a Delete (and Redo of a paste or copy): each image and its sidecars
    back from the trash to where they were, then what its delete purged from the
    stores, then the rows. An image that cannot come back (the trash emptied, its name
    taken) is skipped and named; the rest still come back.
*/
    if (G::isLogger) G::log("MW::restoreTrashed", QString::number(items.size()));
    if (fileOpsBusy()) return false;
    CollectionStore &store = CollectionStore::instance();
    QStringList restored, failed;
    QString why;
    for (const TrashedImage &t : items) {
        if (t.files.isEmpty()) continue;
        QString w;
        const auto &image = t.files.first();
        if (!FileOps::restoreFromTrash(image.second, image.first, &w)) {
            failed << QFileInfo(t.path).fileName();
            why = w;
            G::issue("Warning", "Could not put the image back: " + w + ".",
                     "MW::restoreTrashed", -1, t.path);
            continue;
        }
        for (int i = 1; i < t.files.size(); ++i) {
            const auto &f = t.files.at(i);
            if (!FileOps::restoreFromTrash(f.second, f.first, &w))
                G::issue("Warning", "Put the image back but not its sidecar: " + w + ".",
                         "MW::restoreTrashed", -1, f.first);
        }
        for (auto it = t.collections.cbegin(); it != t.collections.cend(); ++it)
            for (qint64 id : it.value()) store.addMembers(id, {it.key()});
        for (const QString &k : t.savedPicks) SavedPicks::Set(k, true);
        if (t.duplicateOk) DuplicateOk::Set(t.path, true);
        for (const TrashedImage::History &h : t.history)
            HistoryStore::save(h.key, h.entries, h.pos, h.stamp);
        restored << t.path;
    }

    if (!restored.isEmpty()) {
        FileOps::onCreated(restored);
        showRestoredFiles(restored, "MW::restoreTrashed");
        // a reject lives only in the model: put it, and the picks, back on the rows
        PathValues picks;
        for (const TrashedImage &t : items)
            for (const auto &pv : t.pickState)
                if (dm->rowFromKey(pv.first) >= 0) picks << pv;
        if (!picks.isEmpty()) applyPickStates(picks);
    }
    if (!failed.isEmpty()) {
        QString msg = failed.size() == 1
            ? "Could not put back " + failed.first().toHtmlEscaped() + ": " + why + "."
            : QString::number(failed.size()) + " images could not be put back. See the "
              "Issues log.";
        G::popup->showPopup(msg, 4000);
    }
    return !restored.isEmpty();
}

void MW::showRestoredFiles(const QStringList &paths, const QString &src)
{
/*
    Files that are back on disk (an undone delete or move): into the model when what is
    loaded includes them -- the Library, or a loaded folder -- and the folder counts.
*/
    if (G::isLogger) G::log("MW::showRestoredFiles", src);
    if (paths.isEmpty()) return;
    QStringList rows;
    for (const QString &p : paths) {
        const QString folder = QFileInfo(p).absolutePath();
        const bool loaded = G::scope == G::Scope::Catalog
                                ? isCatalogScopeFolder(folder)
                                : dm->folderList.contains(folder);
        if (loaded && dm->rowFromKey(p) < 0) rows << p;
    }
    if (!rows.isEmpty()) {
        G::ScrollSignalGuard scrollGuard;
        applyModelChange(rows, QStringList(), src);
        reconcileVersionRows(src);
    }
    if (G::scope == G::Scope::Catalog) catalogCreatedFilesThenRefreshLibrary();
    QSet<QString> folders;
    for (const QString &p : paths) folders.insert(QFileInfo(p).absolutePath());
    for (const QString &f : std::as_const(folders)) fsTree->updateAFolderCount(f);
    if (bookmarks) bookmarks->updateCount();
}

void MW::recordTrashStep(const QString &what, const QList<TrashedImage> &kept,
                         const QHash<QString, QList<QPair<QString, QString>>> &inTrash)
{
/*
    Push a Delete: undo restores, redo trashes again (and keeps the new trash places).
    kept is keepForTrashUndo's, taken before the trash; inTrash is TrashResult's.
*/
    auto items = std::make_shared<QList<TrashedImage>>();
    for (TrashedImage t : kept) {
        const auto it = inTrash.constFind(t.path);
        if (it == inTrash.cend()) continue;         // not trashed
        t.files = it.value();
        *items << t;
    }
    if (items->isEmpty()) return;
    undoManager->push(new CheckedCommand(
        undoStepText(what, items->size()),
        [this, items] { return restoreTrashed(*items); },
        [this, items] { return trashForUndo(*items); }));
}

void MW::recordNewFilesStep(const QString &what, const QStringList &paths)
{
/*
    Files Winnow just made (a paste, a copy onto a Folders row): undo trashes them, redo
    brings them back from the trash. Nothing of theirs is in a store yet, so the record
    is filled when undo trashes them.
*/
    if (paths.isEmpty()) return;
    auto items = std::make_shared<QList<TrashedImage>>();
    for (const QString &p : paths) {
        TrashedImage t;
        t.path = p;
        *items << t;
    }
    undoManager->push(new CheckedCommand(
        undoStepText(what, paths.size()),
        [this, items] { return trashForUndo(*items); },
        [this, items] { return restoreTrashed(*items); }));
}

void MW::renameModelRow(const QString &oldPath, const QString &newPath)
{
/*
    A loaded image renamed on disk: its row, ImageCache entry and versions follow --
    what RenameFileDlg::renameDatamodel does, for Edit > Undo of a rename.
*/
    if (!dm->fPathRow.contains(oldPath)) return;
    const int row = dm->fPathRow.value(oldPath);
    dm->fPathRow.remove(oldPath);
    dm->fPathRow[newPath] = row;
    const QModelIndex pathIdx = dm->index(row, G::PathColumn);
    const QModelIndex nameIdx = dm->index(row, G::NameColumn);
    if (pathIdx.isValid()) dm->setData(pathIdx, newPath, G::KeyRole);
    if (nameIdx.isValid()) dm->setData(nameIdx, QFileInfo(newPath).fileName());
    if (imageCache) imageCache->rename(oldPath, newPath);
    for (const auto &kv : dm->rekeyVersions(oldPath, newPath))
        if (imageCache) imageCache->rename(kv.first, kv.second);
}

bool MW::applyFileRenames(const QList<QPair<QString, QString>> &renames)
{
/*
    Rename each file from -> to, IN ORDER, keeping the stores (FileOps::onMoved) and the
    loaded rows in step. Undo of the rename dialog passes its log reversed and swapped,
    which retraces every intermediate name it used; redo passes the log as it was.
*/
    if (G::isLogger) G::log("MW::applyFileRenames", QString::number(renames.size()));
    if (fileOpsBusy()) return false;
    FileOps::flushPendingEdits();
    int done = 0;
    QStringList failed;
    for (const auto &r : renames) {
        if (!QFileInfo::exists(r.first) || !renameExact(r.first, r.second)) {
            failed << QFileInfo(r.first).fileName();
            G::issue("Warning", "Could not rename back.", "MW::applyFileRenames", -1,
                     r.first);
            continue;
        }
        FileOps::onMoved(r.first, r.second);
        renameModelRow(r.first, r.second);
        ++done;
    }
    if (dm->currentSfIdx.isValid())
        dm->currentKey = dm->currentSfIdx.data(G::KeyRole).toString();
    titleFilePath = dm->currentSourcePath();
    updateWindowTitle();
    if (!failed.isEmpty())
        G::popup->showPopup(QString::number(failed.size()) + " file(s) could not be "
                            "renamed. See the Issues log.", 4000);
    return done > 0;
}

bool MW::relocateImages(const QList<QPair<QString, QString>> &moves, bool copy,
                        const QString &src)
{
/*
    Move (or copy) each image src -> dst with its sidecars and bring the Library along:
    Edit > Undo and Redo of a drop on a Folders row (MW::moveImagesToFolder). Only in
    the Library, which is the only place that drop exists.
*/
    if (G::isLogger) G::log("MW::relocateImages", src);
    if (fileOpsBusy()) return false;
    if (G::scope != G::Scope::Catalog) {
        G::popup->showPopup("Return to the Library to undo or redo this.", 2500);
        return false;
    }
    QStringList moved, added, failed;
    for (const auto &m : moves) {
        if (!QFileInfo::exists(m.first) || QFileInfo::exists(m.second)) {
            failed << m.first;
            continue;
        }
        if (copy ? FileOps::copyFile(m.first, m.second)
                 : FileOps::moveFile(m.first, m.second)) {
            moved << m.first;
            added << m.second;
        }
        else failed << m.first;
    }
    if (!moved.isEmpty()) afterImagesRelocated(moved, added, QStringList(), copy, src);
    if (!failed.isEmpty()) {
        G::issue("Warning", QString::number(failed.size()) + " image(s) were not where "
                 "the step left them, or their destination is taken.", src);
        G::popup->showPopup(QString::number(failed.size()) + " image(s) could not be "
                            "moved. See the Issues log.", 4000);
    }
    return !moved.isEmpty();
}
