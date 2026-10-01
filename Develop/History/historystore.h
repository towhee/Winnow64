#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QString>
#include <QVector>
#include "Develop/History/historyentry.h"

/*
    Develop history that survives a restart: one row per image in the local index
    database (Cache/cachedb.h, table develop_history), holding every step's snapshot as
    compressed JSON plus the position of the step in force.

    WHY index.db AND NOT THE SIDECAR. The sidecar is a published format that lives beside
    the user's originals and travels with them; it holds the RESULT, and every step is a
    whole EditStack, so a 68-spot image's history would be tens of times its recipe. The
    index is local, rebuildable and already keyed on the same normalised path
    (Cache/pathkey.h). Losing it costs the user their undo steps, never their edits --
    which is the bar for what may live there.

    TRUST, BUT CHECK. Each row carries the stamp of the recipe written to the sidecar in
    the same flush, and is only used while the sidecar still holds that recipe
    (DevelopHistory::seed). Anything that changed the sidecar without this index seeing
    it -- another machine, a restored backup -- makes the history describe a different
    image, and it is ignored.

    BOUNDED: kMaxImages rows, the least recently saved pruned first. Any thread (CacheDb
    hands each thread its own connection); every call degrades to a no-op when the
    database is unavailable.
*/
namespace HistoryStore {

constexpr int kMaxImages = 5000;

/* path's saved history, position and recipe stamp; false when there is none (or it
   is unreadable). */
bool load(const QString &path, QVector<HistoryEntry> &entries, int &pos,
          QString &recipeStamp);
/* Replace path's saved history; recipeStamp is HistoryEntry::recipeStamp of the recipe
   written to the sidecar in the same flush. An empty list removes it. */
void save(const QString &path, const QVector<HistoryEntry> &entries, int pos,
          const QString &recipeStamp);
void remove(const QString &path);

/* Keep in step with file operations Winnow performs itself (FileOps), versions included
   (their keys are the master's path + "/#v<id>"). */
void onMoved(const QString &srcPath, const QString &dstPath);
void onDeleted(const QString &path);

}   // namespace HistoryStore

#endif // HISTORYSTORE_H
