#include "Utilities/fileops.h"
#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Cache/devpreviewcache.h"
#include "Cache/thumbcache.h"
#include "Main/global.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSqlDatabase>

std::function<void()> FileOps::flushHook;
std::function<bool(const QString &)> FileOps::trashHook;

void FileOps::setFlushHook(std::function<void()> hook)
{
    flushHook = std::move(hook);
}

void FileOps::flushPendingEdits()
{
    if (flushHook) flushHook();
}

const QStringList &FileOps::sidecarSuffixes()
{
    static const QStringList suffixes = {"xmp", "txt"};
    return suffixes;
}

namespace {

/* Every sidecar in folder, keyed by lower-cased base name. Only the sidecar suffixes are
   listed (QDir name filters are case-insensitive, so .XMP from another application still
   matches), which keeps the listing to the sidecars rather than a stat of every image in
   the folder. */
using SidecarIndex = QHash<QString, QStringList>;

SidecarIndex sidecarIndex(const QDir &folder)
{
    QStringList filters;
    for (const QString &suffix : FileOps::sidecarSuffixes()) filters << "*." + suffix;
    SidecarIndex index;
    const auto files = folder.entryInfoList(filters, QDir::Files | QDir::Hidden);
    for (const QFileInfo &f : files) {
        if (!FileOps::sidecarSuffixes().contains(f.suffix().toLower())) continue;
        index[f.baseName().toLower()] << f.absoluteFilePath();
    }
    return index;
}

QStringList companionsFrom(const SidecarIndex &index, const QFileInfo &info)
{
    const QString base = info.baseName();
    if (base.isEmpty()) return {};
    const QString self = info.absoluteFilePath();
    QStringList result;
    for (const QString &s : index.value(base.toLower()))
        if (s != self) result << s;
    return result;
}

}  // namespace

QStringList FileOps::companions(const QString &fPath)
{
    if (fPath.isEmpty()) return {};
    const QFileInfo info(fPath);
    return companionsFrom(sidecarIndex(info.absoluteDir()), info);
}

namespace {

/* Companion destination for a companion of srcPath moving to dstPath. The companion
   follows the destination's base name, so renaming DSC_001.NEF to Sunset.NEF takes
   DSC_001.xmp to Sunset.xmp. */
QString companionDest(const QString &companion, const QString &dstPath)
{
    const QFileInfo ci(companion);
    const QFileInfo di(dstPath);
    return di.absoluteDir().absoluteFilePath(di.baseName() + "." + ci.suffix());
}

}  // namespace

/*
    The develop preview cache folder is browsable but read-only: its files are named by an
    opaque id that only the cache index can attribute to an image, and the cache deletes
    any file its index does not name. Renaming, moving or trashing in there detaches
    previews from their images; copying in there drops the copy (and its sidecar) at the
    next reconcile. So every operation that writes refuses on either side of the path.
    See DevPreviewCache::isCachePath.
*/
static bool isProtected(const QString &path, const QString &src)
{
    if (!DevPreviewCache::instance().isCachePath(path)) return false;
    G::issue("Warning", "Refusing to write: "
             + DevPreviewCache::readOnlyReason() + ".", src, -1, path);
    return true;
}

bool FileOps::copyFile(const QString &srcPath, const QString &dstPath)
{
    if (G::isLogger) G::log("FileOps::copyFile");
    if (isProtected(dstPath, "FileOps::copyFile")) return false;
    flushPendingEdits();

    if (!QFile::copy(srcPath, dstPath)) {
        QString msg = "Failed to copy file.";
        G::issue("Warning", msg, "FileOps::copyFile", -1, srcPath);
        return false;
    }

    const auto sidecars = companions(srcPath);
    for (const QString &s : sidecars) {
        const QString dst = companionDest(s, dstPath);
        if (QFile::exists(dst)) QFile::remove(dst);
        if (!QFile::copy(s, dst)) {
            QString msg = "Copied the image but failed to copy its sidecar.";
            G::issue("Warning", msg, "FileOps::copyFile", -1, s);
        }
    }

    onCopied(srcPath, dstPath);
    return true;
}

bool FileOps::moveFile(const QString &srcPath, const QString &dstPath)
{
    if (G::isLogger) G::log("FileOps::moveFile");
    if (isProtected(srcPath, "FileOps::moveFile")) return false;
    if (isProtected(dstPath, "FileOps::moveFile")) return false;
    flushPendingEdits();

    /* Companions first: if the image move fails we have not orphaned anything, because
       a companion whose image never moved is still findable at the source. */
    const auto sidecars = companions(srcPath);

    if (QFile::exists(dstPath)) QFile::remove(dstPath);
    if (!QFile::rename(srcPath, dstPath)) {
        QString msg = "Failed to move file.";
        G::issue("Warning", msg, "FileOps::moveFile", -1, srcPath);
        return false;
    }

    for (const QString &s : sidecars) {
        const QString dst = companionDest(s, dstPath);
        if (QFile::exists(dst)) QFile::remove(dst);
        if (!QFile::rename(s, dst)) {
            QString msg = "Moved the image but failed to move its sidecar.";
            G::issue("Warning", msg, "FileOps::moveFile", -1, s);
        }
    }

    onMoved(srcPath, dstPath);
    return true;
}

bool FileOps::trashFile(const QString &fPath)
{
    if (G::isLogger) G::log("FileOps::trashFile");
    return trashFiles({fPath}).trashed.size() == 1;
}

void FileOps::setTrashHook(std::function<bool(const QString &)> hook)
{
    trashHook = std::move(hook);
}

bool FileOps::moveOneToTrash(const QString &path)
{
    if (trashHook) return trashHook(path);
    return QFile::moveToTrash(path);
}

FileOps::TrashResult FileOps::trashFiles(const QStringList &paths,
                                         const std::function<bool(int)> &progress)
{
    if (G::isLogger) G::log("FileOps::trashFiles", QString::number(paths.size()));
    TrashResult result;
    if (paths.isEmpty()) return result;
    flushPendingEdits();

    /* Sized so a chunk is a fraction of a second of OS trash calls: often enough for a
       responsive progress bar and cancel, rarely enough that the per-chunk transaction
       and repaint are noise. */
    const int chunkSize = 100;

    QHash<QString, SidecarIndex> folders;       // folder path -> its sidecars
    QStringList chunkTrashed;
    int done = 0;

    for (const QString &fPath : paths) {
        const QFileInfo info(fPath);
        if (isProtected(fPath, "FileOps::trashFiles")) {
            result.failed << fPath;
        }
        else if (!info.exists()) {
            G::issue("Warning", "File does not exist.", "FileOps::trashFiles", -1, fPath);
            result.missing << fPath;
        }
        else {
            const QString folder = info.absolutePath();
            auto it = folders.find(folder);
            if (it == folders.end())
                it = folders.insert(folder, sidecarIndex(info.absoluteDir()));
            const QStringList sidecars = companionsFrom(it.value(), info);

            if (!moveOneToTrash(fPath)) {
                QString msg = info.isWritable() ? "Unable to move to trash."
                                                : "File is locked. Unable to move to trash.";
                G::issue("Warning", msg, "FileOps::trashFiles", -1, fPath);
                result.failed << fPath;
            }
            else {
                /* Only once the image is gone -- a sidecar whose image survived would
                   lose every develop edit for an image still in the folder. */
                for (const QString &s : sidecars) {
                    if (QFile::exists(s) && !moveOneToTrash(s)) {
                        QString msg = "Trashed the image but could not trash its sidecar.";
                        G::issue("Warning", msg, "FileOps::trashFiles", -1, s);
                    }
                }
                chunkTrashed << fPath;
            }
        }

        ++done;
        if (done % chunkSize == 0 || done == paths.size()) {
            /* One transaction for the chunk's cache rows. The three caches share this
               thread's CacheDb connection, so their statements all join it. */
            QSqlDatabase db = CacheDb::instance().db();
            const bool tx = db.isOpen() && db.transaction();
            for (const QString &p : std::as_const(chunkTrashed)) onDeleted(p);
            if (tx) db.commit();
            result.trashed << chunkTrashed;
            chunkTrashed.clear();

            if (progress && !progress(done) && done < paths.size()) {
                result.cancelled = true;
                break;
            }
        }
    }
    return result;
}

/* ---------------------------------------------------------------------------------
   Cache notifications

   Tier 1 (the 256px thumbnail preview) needs nothing here: it lives inside the sidecar,
   so it is carried by whatever moved the sidecar. Only the Tier 2 loupe cache, which is
   out of band, has to be told.
   --------------------------------------------------------------------------------- */

void FileOps::onCopied(const QString &srcPath, const QString &dstPath)
{
    Q_UNUSED(srcPath)
    Q_UNUSED(dstPath)
    /* Deliberately not duplicated. The copy carries its sidecar, so the thumbnail
       preview travels; the loupe preview simply misses at the destination and is
       re-rendered the next time that image is edited. Duplicating it would double the
       cache for every copy to buy back one ~2s decode. */
}

void FileOps::onMoved(const QString &srcPath, const QString &dstPath)
{
    DevPreviewCache::instance().onMoved(srcPath, dstPath);
    ThumbCache::instance().onMoved(srcPath, dstPath);
    /* The catalog keys on the path too, so a move Winnow performs itself must follow
       the image -- otherwise a search keeps offering the old location, and loading the
       result fails. */
    Catalog::instance().onMoved(srcPath, dstPath);
}

void FileOps::onDeleted(const QString &fPath)
{
    DevPreviewCache::instance().onDeleted(fPath);
    ThumbCache::instance().onDeleted(fPath);
    Catalog::instance().onDeleted(fPath);
}
