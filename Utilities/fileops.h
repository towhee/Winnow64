#ifndef FILEOPS_H
#define FILEOPS_H

#include <QString>
#include <QStringList>
#include <functional>

class QRecursiveMutex;

/*
    The single place every on-disk image file operation goes through.

    WHY THIS EXISTS

    An image in Winnow is not one file. It is the image plus its companions -- the XMP
    sidecar that holds ratings, labels, orientation and the entire Develop recipe, and a
    .txt sidecar if one is present. The sidecar IS Winnow's per-image database: there is
    no catalogue, so losing a sidecar loses every edit ever made to that image.

    Before this class, sidecar handling was implemented four different ways (a .xmp+.txt
    helper used by 4 call sites, a .xmp-only helper used by 1, a directory basename scan
    used by rename, and hardcoded baseName + ".xmp" in ingest and the metadata writers).
    They disagreed about .txt files, about case, and about whether an externally-dropped
    file had a sidecar at all. Adding cached develop previews -- a second piece of
    per-image state that must track the first exactly -- made one definition mandatory.

    WHAT CALLERS GET

    - companions()      one definition of "the sidecars belonging to this image"
    - flushPendingEdits()  no operation may run while an edit is still debounced
    - copy/move/rename/trash  do the whole job: image, companions, preview cache
    - onCopied/onMoved/onDeleted  for callers that must move the bytes themselves
      (the rename dialog and ingest both rename companions to a NEW basename, which the
      generic helpers cannot express)

    THE FLUSH IS NOT OPTIONAL

    Develop edits are written to the sidecar on a 2s debounce
    (DevelopProperties::flushImage). Before this class only copy and export flushed
    first, so deleting, renaming, moving or ingesting an image inside that window let the
    pending write land afterwards -- recreating a sidecar at the OLD path after a rename,
    or resurrecting one that was just deleted. Every entry point here flushes first.

    THREADING

    GUI thread only. The flush hook reaches into DevelopProperties, which is a widget.
*/
class FileOps
{
public:
    /* Set once at startup by MW. Keeping this a hook rather than a direct call lets the
       file operations be unit-tested without a MainWindow, and keeps Utilities free of a
       dependency on Develop. */
    static void setFlushHook(std::function<void()> hook);

    /* Persist any debounced Develop edits. Called at the head of every operation below;
       call it directly before any file work this class does not yet cover. */
    static void flushPendingEdits();

    /* SIDECAR NAMING -- the one definition. See "Sidecar naming" in
       notes/Documentation.txt.

       Raw and HEIC:            IMG_1.NEF  -> IMG_1.xmp      (Lightroom's name)
       JPEG, TIFF, PNG, DNG:    IMG_1.JPG  -> IMG_1.JPG.xmp  (full-name sidecar)

       Lightroom writes XMP INTO the full-name formats and ignores any sidecar beside
       them, so there IMG_1.xmp is always another file's -- a raw+JPEG pair shares that
       name, and a JPEG edit written there used to change the raw's Lightroom metadata.
       The full-name sidecar cannot collide. It is Winnow's own: it holds the Develop
       recipe, and the standard fields while "Permit image file modification" is off.

       LEGACY. Winnow once wrote IMG_1.xmp for every format. A full-name image with no
       IMG_1.JPG.xmp still READS an IMG_1.xmp, unless a raw/HEIC (any sibling of another
       format) shares the base name -- then it is that file's and is left alone. The
       first write adopts it (prepareSidecarForWrite). */
    static bool usesFullNameSidecar(const QString &fPath);

    /* Where Winnow writes fPath's XMP sidecar. May not exist. */
    static QString sidecarPath(const QString &fPath);

    /* The sidecar to READ for fPath: sidecarPath() if it exists, else a readable legacy
       IMG_1.xmp (see above), else "". Safe on worker threads. */
    static QString existingSidecar(const QString &fPath);

    /* Call before writing fPath's sidecar; returns sidecarPath(). If only a readable
       legacy IMG_1.xmp exists it is carried to the new name first, so the write updates
       it rather than starting an empty document beside it. Moved when fPath is the only
       file with that base name, copied when another full-name image (the DNG of a
       DNG+JPG pair) may still read it. */
    static QString prepareSidecarForWrite(const QString &fPath);

    /* The sidecars belonging to fPath: its full-name sidecars (IMG_1.JPG.xmp/.txt), and
       the base-name ones (IMG_1.xmp/.txt) when they are fPath's -- always for a raw or
       HEIC, and for a full-name format only when no other file shares the base name.
       Matched case-insensitively so a .XMP written by another application is not
       missed. Existence-filtered.

       Deliberately NOT every file sharing the base name -- that would sweep in the
       paired JPG of a raw+jpg pair, and trashing a NEF must not trash its JPG. Nor may
       trashing the JPG take the NEF's IMG_1.xmp. Rename is the one operation that does
       want the wider net, and it keeps its own scan. */
    static QStringList companions(const QString &fPath);

    /* Where companion of srcPath goes when srcPath becomes dstPath. A base-name sidecar
       follows the destination's base name (DSC_001.xmp -> Sunset.xmp); a full-name one
       follows its file name (DSC_001.JPG.xmp -> Sunset.JPG.xmp). */
    static QString companionDest(const QString &companion, const QString &srcPath,
                                 const QString &dstPath);

    /* Full operations: the image, its companions, and the preview cache. Return true
       when the IMAGE itself was handled; a companion failure is reported but does not
       fail the operation, since the image has already moved. */
    static bool copyFile(const QString &srcPath, const QString &dstPath);
    static bool moveFile(const QString &srcPath, const QString &dstPath);
    static bool trashFile(const QString &fPath);

    /* BATCH TRASH. The same job as trashFile for every path, but priced per file rather
       than per folder: the folder listing that finds sidecars is made once per folder
       (not once per image, which made a 7,000 image delete O(N^2)), the Develop flush
       runs once, and the cache notifications are committed one transaction per chunk.
       progress(done) is called after each chunk; returning false stops the batch
       between chunks, and what was trashed so far is still reported. */
    struct TrashResult {
        QStringList trashed;        // images that went, in the order given
        QStringList missing;        // already gone from disk -- issued, not trashed
        QStringList failed;         // protected, locked, refused -- each already issued
        bool cancelled = false;
    };
    static TrashResult trashFiles(const QStringList &paths,
                                  const std::function<bool(int done)> &progress = {});

    /* Test seam: what actually moves one file to the trash. Defaults to
       QFile::moveToTrash; the unit tests point it at a temp folder so the batch can be
       exercised without filling the user's real Trash. Pass an empty function to
       restore the default. */
    static void setTrashHook(std::function<bool(const QString &)> hook);

    /* Notifications, for callers that move the bytes themselves. These do NOT touch the
       image or its companions -- they only bring the caches into line. */
    static void onCopied(const QString &srcPath, const QString &dstPath);
    static void onMoved(const QString &srcPath, const QString &dstPath);
    static void onDeleted(const QString &fPath);
    /* A whole FOLDER Winnow removed (Delete Folder, erase a memory card, focus-stack
       work folders): its catalog rows and everything below it are deleted, as
       onDeleted does for one image -- and the same for every store onDeleted tells:
       thumbnails, devPreviews (payload files included), collection membership and
       Develop history. Returns the catalog rows removed, so a GUI caller knows whether
       the Library needs redrawing. Any thread. */
    static int onFolderDeleted(const QString &folder);

    /* NEW IMAGE FILES ON DISK -- an export, an ingest copy, a stacked or embellished
       result. Call once the file is COMPLETE (metadata copied, sidecar written), since
       what is notified may be indexed straight away. onCopied and onMoved call it for
       their destination, so callers of those need not.

       It exists so the catalog can take in an image Winnow itself just made in a
       folder the scope table admits, instead of leaving it unsearchable until the next
       scan. Any thread: the hook only queues the paths. See "New and Deleted Files
       Follow the Catalog" in notes/Documentation.txt. */
    static void onCreated(const QStringList &paths);

    /* Set once at startup by MW, which owns the scope table. Must be safe to call from
       any thread. Unset (unit tests), onCreated does nothing. */
    static void setCreatedHook(std::function<void(const QStringList &)> hook);

    /* VERSION KEY GUARD. A DataModel row key for a version (path + "/#v" + id, see
       Utilities/versionkey.h) is not a file. Returns true, and raises G::issue, when
       path is one -- the caller must then refuse the operation. Every file entry point
       here calls it, and so do the metadata and image readers/writers, so a key that
       slips past the G::KeyRole / G::SourcePathRole split fails loudly instead of
       writing into some other file. */
    static bool refuseVersionKey(const QString &path, const QString &src);

    /* SIDECAR WRITE LOCK. Every read-modify-write of an XMP sidecar holds this for the
       image's source path: the standard-field writer (Metadata::writeXMP, GUI thread),
       the orientation writer (QtConcurrent) and the Develop and version writers all
       rewrite the whole document, so two unlocked writers lose one's changes. Keyed by
       cachePathKey; the mutexes live for the process. Recursive, because writeXMP and
       writeKeywordsToSidecar reach markSidecarEmbedded while holding it. */
    static QRecursiveMutex &sidecarLock(const QString &fPath);

    /* The suffixes companions() recognises, without the dot. */
    static const QStringList &sidecarSuffixes();

    /* The formats that get a full-name sidecar, lower case, without the dot. */
    static const QStringList &fullNameSidecarFormats();

private:
    static std::function<void()> flushHook;
    static std::function<bool(const QString &)> trashHook;
    static std::function<void(const QStringList &)> createdHook;
    static bool moveOneToTrash(const QString &path);
};

#endif // FILEOPS_H
