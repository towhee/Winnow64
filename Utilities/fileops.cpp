#include "Utilities/fileops.h"
#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Cache/devpreviewcache.h"
#include "Cache/thumbcache.h"
#include "Datamodel/collectionstore.h"
#include "Develop/History/historystore.h"
#include "Main/global.h"
#include "Cache/pathkey.h"
#include "Utilities/versionkey.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>

std::function<void()> FileOps::flushHook;
std::function<bool(const QString &)> FileOps::trashHook;
std::function<void(const QStringList &)> FileOps::createdHook;

void FileOps::setFlushHook(std::function<void()> hook)
{
    flushHook = std::move(hook);
}

/* Guards createdHook. onCreated runs on worker threads (Ingest::run) while MW clears
   the hook on quit; calling it UNDER the lock means that once setCreatedHook({})
   returns, no call can still be reaching into MW. The hook only posts an event, so
   holding the lock across it costs nothing. */
static QMutex createdHookMutex;

void FileOps::setCreatedHook(std::function<void(const QStringList &)> hook)
{
    QMutexLocker lk(&createdHookMutex);
    createdHook = std::move(hook);
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

const QStringList &FileOps::fullNameSidecarFormats()
{
    static const QStringList formats = {"jpg", "jpeg", "tif", "tiff", "png", "dng"};
    return formats;
}

namespace {

/* Lower-cased name split at the LAST dot: "IMG_1.JPG.xmp" -> ("img_1.jpg", "xmp").
   QFileInfo::baseName stops at the FIRST dot, which would file IMG_1.JPG.xmp under
   IMG_1 -- the raw's name -- and is exactly the confusion full-name sidecars exist to
   avoid. */
void splitName(const QString &name, QString &base, QString &suffix)
{
    const int dot = name.lastIndexOf('.');
    if (dot <= 0) { base = name.toLower(); suffix.clear(); return; }
    base = name.left(dot).toLower();
    suffix = name.mid(dot + 1).toLower();
}

/* Who else in a folder answers to a base name. images counts every non-sidecar file;
   rawOwner is set when one of them is not a full-name format, which makes the
   base-name IMG_1.xmp that file's. */
struct Claim {
    int images = 0;
    bool rawOwner = false;
};

/* One folder, listed once (names only -- no stat per file):
   sidecars  every sidecar, keyed by its lower-cased complete base name, so IMG_1.xmp is
             under "img_1" and IMG_1.JPG.xmp under "img_1.jpg"
   claims    every image's base name */
struct FolderIndex {
    QHash<QString, QStringList> sidecars;
    QHash<QString, Claim> claims;
};

FolderIndex folderIndex(const QDir &folder)
{
    FolderIndex index;
    const QStringList names = folder.entryList(QDir::Files | QDir::Hidden, QDir::NoSort);
    QString base, suffix;
    for (const QString &name : names) {
        splitName(name, base, suffix);
        if (FileOps::sidecarSuffixes().contains(suffix)) {
            index.sidecars[base] << folder.absoluteFilePath(name);
            continue;
        }
        Claim &c = index.claims[base];
        ++c.images;
        if (!FileOps::fullNameSidecarFormats().contains(suffix)) c.rawOwner = true;
    }
    return index;
}

/* The claims for info's folder, for the per-image read path. Folder load asks this once
   per JPEG of a raw+JPEG folder, so the listing is cached and re-taken only when the
   folder's mtime moves (any file added, removed or renamed). Worker threads call it. */
Claim claimFor(const QFileInfo &info)
{
    struct Entry { QDateTime mtime; QHash<QString, Claim> claims; };
    static QMutex mutex;
    static QHash<QString, Entry> cache;

    const QString folder = info.absolutePath();
    const QDateTime mtime = QFileInfo(folder).lastModified();
    QString base, suffix;
    splitName(info.fileName(), base, suffix);

    QMutexLocker lock(&mutex);
    auto it = cache.find(folder);
    if (it == cache.end() || it->mtime != mtime) {
        // a bound, not an LRU: a rebuild is one names-only listing
        if (cache.size() > 64) cache.clear();
        it = cache.insert(folder, {mtime, folderIndex(info.absoluteDir()).claims});
    }
    return it->claims.value(base);
}

QString legacySidecarPath(const QFileInfo &info)
{
    return info.absoluteDir().absoluteFilePath(info.completeBaseName() + ".xmp");
}

QStringList companionsFrom(const FolderIndex &index, const QFileInfo &info)
{
    if (info.fileName().isEmpty()) return {};
    QString base, suffix;
    splitName(info.fileName(), base, suffix);
    const QString self = info.absoluteFilePath();
    QStringList result;

    // full-name: IMG_1.JPG.xmp -- also darktable's IMG_1.NEF.xmp
    for (const QString &s : index.sidecars.value(info.fileName().toLower()))
        if (s != self) result << s;

    // base-name: IMG_1.xmp -- a raw's own; a full-name image's only if it is alone
    const bool ownsBase = !FileOps::fullNameSidecarFormats().contains(suffix)
                          || index.claims.value(base).images <= 1;
    if (ownsBase) {
        for (const QString &s : index.sidecars.value(base))
            if (s != self) result << s;
    }
    result.removeDuplicates();      // an extension-less name is its own base
    return result;
}

}  // namespace

bool FileOps::usesFullNameSidecar(const QString &fPath)
{
    return fullNameSidecarFormats().contains(QFileInfo(fPath).suffix().toLower());
}

bool FileOps::refuseVersionKey(const QString &path, const QString &src)
{
    if (!VersionKey::isVersion(path)) return false;
    G::issue("Error", "A version key reached file I/O; refused.", src, -1, path);
    return true;
}

QRecursiveMutex &FileOps::sidecarLock(const QString &fPath)
{
    static QMutex mapLock;
    static QHash<QString, QRecursiveMutex *> locks;     // never freed: one per image touched
    const QString key = cachePathKey(VersionKey::sourceOf(fPath));
    QMutexLocker locker(&mapLock);
    QRecursiveMutex *&m = locks[key];
    if (!m) m = new QRecursiveMutex;
    return *m;
}

QString FileOps::sidecarPath(const QString &fPath)
{
    if (refuseVersionKey(fPath, "FileOps::sidecarPath")) return QString();
    const QFileInfo info(fPath);
    if (usesFullNameSidecar(fPath))
        return info.absoluteDir().absoluteFilePath(info.fileName() + ".xmp");
    return legacySidecarPath(info);
}

QString FileOps::existingSidecar(const QString &fPath)
{
    if (fPath.isEmpty()) return QString();
    if (refuseVersionKey(fPath, "FileOps::existingSidecar")) return QString();
    const QString path = sidecarPath(fPath);
    if (QFileInfo::exists(path)) return path;
    if (!usesFullNameSidecar(fPath)) return QString();

    const QFileInfo info(fPath);
    const QString legacy = legacySidecarPath(info);
    if (!QFileInfo::exists(legacy)) return QString();
    return claimFor(info).rawOwner ? QString() : legacy;
}

QString FileOps::prepareSidecarForWrite(const QString &fPath)
{
    const QString path = sidecarPath(fPath);
    if (path.isEmpty()) return path;               // refused (a version key)
    const QString existing = existingSidecar(fPath);
    if (existing.isEmpty() || existing == path) return path;

    /* existing is a legacy IMG_1.xmp this image reads. Never follow a symlink -- the
       writers refuse one, and a copy or rename here would launder it. */
    if (QFileInfo(existing).isSymLink()) return path;
    const bool alone = claimFor(QFileInfo(fPath)).images <= 1;
    const bool ok = alone ? QFile::rename(existing, path) : QFile::copy(existing, path);
    if (!ok) {
        G::issue("Warning", "Could not carry the old sidecar to its new name.",
                 "FileOps::prepareSidecarForWrite", -1, existing);
    }
    return path;
}

QStringList FileOps::companions(const QString &fPath)
{
    if (fPath.isEmpty()) return {};
    if (refuseVersionKey(fPath, "FileOps::companions")) return {};
    const QFileInfo info(fPath);
    return companionsFrom(folderIndex(info.absoluteDir()), info);
}

QString FileOps::companionDest(const QString &companion, const QString &srcPath,
                               const QString &dstPath)
{
    const QFileInfo ci(companion);
    const QFileInfo si(srcPath);
    const QFileInfo di(dstPath);
    const bool fullName =
        ci.completeBaseName().compare(si.fileName(), Qt::CaseInsensitive) == 0;
    const QString stem = fullName ? di.fileName() : di.completeBaseName();
    return di.absoluteDir().absoluteFilePath(stem + "." + ci.suffix());
}

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
    if (refuseVersionKey(srcPath, "FileOps::copyFile")) return false;
    if (refuseVersionKey(dstPath, "FileOps::copyFile")) return false;
    if (isProtected(dstPath, "FileOps::copyFile")) return false;
    flushPendingEdits();

    if (!QFile::copy(srcPath, dstPath)) {
        QString msg = "Failed to copy file.";
        G::issue("Warning", msg, "FileOps::copyFile", -1, srcPath);
        return false;
    }

    const auto sidecars = companions(srcPath);
    for (const QString &s : sidecars) {
        const QString dst = companionDest(s, srcPath, dstPath);
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
    if (refuseVersionKey(srcPath, "FileOps::moveFile")) return false;
    if (refuseVersionKey(dstPath, "FileOps::moveFile")) return false;
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
        const QString dst = companionDest(s, srcPath, dstPath);
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

    QHash<QString, FolderIndex> folders;        // folder path -> its sidecars and claims
    QStringList chunkTrashed;
    int done = 0;

    for (const QString &fPath : paths) {
        const QFileInfo info(fPath);
        if (refuseVersionKey(fPath, "FileOps::trashFiles")
            || isProtected(fPath, "FileOps::trashFiles")) {
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
                it = folders.insert(folder, folderIndex(info.absoluteDir()));
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
    onCreated({dstPath});
}

void FileOps::onMoved(const QString &srcPath, const QString &dstPath)
{
    DevPreviewCache::instance().onMoved(srcPath, dstPath);
    ThumbCache::instance().onMoved(srcPath, dstPath);
    /* The catalog keys on the path too, so a move Winnow performs itself must follow
       the image -- otherwise a search keeps offering the old location, and loading the
       result fails. */
    Catalog::instance().onMoved(srcPath, dstPath);
    /* Collections key on the path as well, and they are the user's own work: an image
       Winnow moves stays in every collection that held it. */
    CollectionStore::instance().onMoved(srcPath, dstPath);
    /* Develop history keys on the path as well; undo steps follow the image. */
    HistoryStore::onMoved(srcPath, dstPath);
    /* A move INTO the catalog scope from outside it is a new image as far as the
       catalog is concerned: Catalog::onMoved had no row to carry. When it did carry one,
       the stamps still match and the indexer passes it over without a parse. */
    onCreated({dstPath});
}

int FileOps::onFolderDeleted(const QString &folder)
{
    if (folder.isEmpty()) return 0;
    /* onDeleted's receivers, by folder. A removal Winnow performed itself is certain,
       so every store deletes rather than demotes -- the same reasoning as
       Catalog::onDeleted. */
    DevPreviewCache::instance().onFolderDeleted(folder);
    ThumbCache::instance().onFolderDeleted(folder);
    CollectionStore::instance().onFolderDeleted(folder);
    HistoryStore::onFolderDeleted(folder);
    return Catalog::instance().forgetUnder(folder, /*recurse*/true);
}

void FileOps::onCreated(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    QMutexLocker lk(&createdHookMutex);
    if (createdHook) createdHook(paths);
}

void FileOps::onDeleted(const QString &fPath)
{
    DevPreviewCache::instance().onDeleted(fPath);
    ThumbCache::instance().onDeleted(fPath);
    Catalog::instance().onDeleted(fPath);
    CollectionStore::instance().onDeleted(fPath);
    HistoryStore::onDeleted(fPath);
}
