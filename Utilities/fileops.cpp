#include "Utilities/fileops.h"
#include "Cache/devpreviewcache.h"
#include "Main/global.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

std::function<void()> FileOps::flushHook;

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

QStringList FileOps::companions(const QString &fPath)
{
    QStringList result;
    if (fPath.isEmpty()) return result;

    const QFileInfo info(fPath);
    const QString base = info.baseName();
    if (base.isEmpty()) return result;

    /* Scan the folder rather than probing "<base>.xmp" directly so a sidecar written in
       a different case (.XMP from another application) is still found. */
    const QDir folder = info.absoluteDir();
    const QString self = info.absoluteFilePath();
    const auto files = folder.entryInfoList(QDir::Files | QDir::Hidden);
    for (const QFileInfo &f : files) {
        if (f.absoluteFilePath() == self) continue;
        if (f.baseName().compare(base, Qt::CaseInsensitive) != 0) continue;
        if (!sidecarSuffixes().contains(f.suffix().toLower())) continue;
        result << f.absoluteFilePath();
    }
    return result;
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
    if (isProtected(fPath, "FileOps::trashFile")) return false;
    flushPendingEdits();

    const auto sidecars = companions(fPath);

    QFile image(fPath);
    if (!image.moveToTrash()) {
        QString msg = "Unable to move to trash.";
        G::issue("Warning", msg, "FileOps::trashFile", -1, fPath);
        return false;
    }

    /* Only once the image is gone -- a sidecar whose image survived would lose every
       develop edit for an image still in the folder. */
    for (const QString &s : sidecars) {
        QFile f(s);
        if (f.exists() && !f.moveToTrash()) {
            QString msg = "Trashed the image but could not trash its sidecar.";
            G::issue("Warning", msg, "FileOps::trashFile", -1, s);
        }
    }

    onDeleted(fPath);
    return true;
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
}

void FileOps::onDeleted(const QString &fPath)
{
    DevPreviewCache::instance().onDeleted(fPath);
}
