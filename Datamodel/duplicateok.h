#ifndef DUPLICATEOK_H
#define DUPLICATEOK_H

#include <QSet>
#include <QString>

/*
    "DUPLICATE OKAY" -- images the user has told the Library duplicates check to ignore
    (Cache/duplicates.h). userdata.db schema 5 (Datamodel/userdb.h), table duplicate_ok:
    the file path of every okayed image. It is a decision, so it lives with the user's
    work and not in the rebuildable index; rebuilding index.db re-finds the duplicates
    and they come back okayed.

    KEYED BY FILE PATH, as the catalog's image.path spells it -- a version (virtual copy)
    is not a file and is never a duplicate, so it is never okayed either.

    The shape is SavedPicks' (Datamodel/savedpicks.h): an in-memory set the app reads,
    written through on the next turn of the event loop in one transaction, and moves,
    renames and deletes Winnow performs follow the image (OnMoved, OnDeleted,
    OnFolderMoved, OnFolderDeleted, from FileOps). A file changed outside Winnow leaves a
    path behind; it matches nothing, so it okays nothing.

    GUI THREAD. Set and Flush are called on it; the FileOps notifications hop to it.
    Nothing happens until Open: MW opens it at startup except in test mode.
*/
namespace DuplicateOk {

void Open();
bool IsOpen();
/* Stop writing and forget the in-memory set (tests). Unwritten changes are flushed. */
void Close();

const QSet<QString> &Paths();
bool IsOk(const QString &path);
int Count();

/* Okay (ok = true) or un-okay one image. No-op when unchanged or closed. */
void Set(const QString &path, bool ok);
/* Write every queued change now, in one transaction. */
void Flush();

void OnMoved(const QString &srcPath, const QString &dstPath);
void OnDeleted(const QString &path);
void OnFolderDeleted(const QString &folder);
void OnFolderMoved(const QString &from, const QString &to);

}   // namespace DuplicateOk

#endif // DUPLICATEOK_H
