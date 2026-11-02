#ifndef SAVEDPICKS_H
#define SAVEDPICKS_H

#include <QSet>
#include <QString>

/*
    PICKS ARE THE USER'S WORK, KEPT LIKE A RATING -- userdata.db schema 4 (Datamodel/
    userdb.h), table saved_pick: the datamodel row key (G::KeyRole: a file path, or a
    version key, Utilities/versionkey.h) of every image picked and not yet ingested.

    WRITE-THROUGH, so a crash or a power loss keeps them. DataModel::setData is the one
    place a pick changes; on every Picked <-> not-Picked transition it calls Set, which
    updates the in-memory set at once and queues the key. The queue is written on the
    next turn of the event loop in ONE transaction (Flush), so a multi-select pick of 500
    images is one commit, and userdata.db's synchronous = FULL makes the commit durable.

    THE IN-MEMORY SET IS WHAT THE APP READS (Keys, Contains, Count): restoring a loaded
    folder (DataModel::syncSavedPicks) and the close warning never touch the database.

    NOT the rebuildable index. The index's image.pick went stale because it was refreshed
    only when a sidecar changed, so an unpick never reached it; a write at the transition
    itself cannot miss one.

    Moves, renames and deletes Winnow performs follow the image (OnMoved, OnDeleted,
    OnFolderDeleted, from FileOps), as for collections. Files changed outside Winnow
    leave a key behind; it matches nothing, so it picks nothing.

    GUI THREAD. Set and Flush are called on it; the FileOps notifications hop to it.
    Nothing happens until Open: MW opens it at startup except in test mode, so
    --selftest and --soaktest start Unpicked and never touch the user's picks.
*/
namespace SavedPicks {

/* Read the saved picks into memory and start writing through. A closed userdata.db
   leaves it working in memory only, for the session. */
void Open();
bool IsOpen();
/* Stop writing and forget the in-memory set (tests). Unwritten changes are flushed. */
void Close();

const QSet<QString> &Keys();
bool Contains(const QString &key);
int Count();

/* A row's pick changed: picked = it is now "Picked". No-op when unchanged or closed. */
void Set(const QString &key, bool picked);
/* Write every queued change now, in one transaction (closeEvent; the queued flush). */
void Flush();
/* Forget every saved pick -- the close dialog's "Clear Picks and Close". */
void ClearAll();

void OnMoved(const QString &srcPath, const QString &dstPath);
void OnDeleted(const QString &path);
void OnFolderDeleted(const QString &folder);

}   // namespace SavedPicks

#endif // SAVEDPICKS_H
