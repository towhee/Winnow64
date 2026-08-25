#ifndef FILEOPS_H
#define FILEOPS_H

#include <QString>
#include <QStringList>
#include <functional>

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

    /* The sidecars belonging to fPath: files in the same folder with the same base name
       and a sidecar suffix (.xmp, .txt), matched case-insensitively so a .XMP written by
       another application is not missed. Existence-filtered.

       Deliberately NOT every file sharing the base name -- that would sweep in the
       paired JPG of a raw+jpg pair, and trashing a NEF must not trash its JPG. Rename is
       the one operation that does want the wider net, and it keeps its own scan. */
    static QStringList companions(const QString &fPath);

    /* Full operations: the image, its companions, and the preview cache. Return true
       when the IMAGE itself was handled; a companion failure is reported but does not
       fail the operation, since the image has already moved. */
    static bool copyFile(const QString &srcPath, const QString &dstPath);
    static bool moveFile(const QString &srcPath, const QString &dstPath);
    static bool trashFile(const QString &fPath);

    /* Notifications, for callers that move the bytes themselves. These do NOT touch the
       image or its companions -- they only bring the caches into line. */
    static void onCopied(const QString &srcPath, const QString &dstPath);
    static void onMoved(const QString &srcPath, const QString &dstPath);
    static void onDeleted(const QString &fPath);

    /* The suffixes companions() recognises, without the dot. */
    static const QStringList &sidecarSuffixes();

private:
    static std::function<void()> flushHook;
};

#endif // FILEOPS_H
