#include "Datamodel/savedpicks.h"
#include "Datamodel/userdb.h"
#include "Utilities/versionkey.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QTimer>
#include <QVariant>

namespace {

bool opened = false;
QSet<QString> keys;                 // what is picked, as far as the app is concerned
QHash<QString, bool> dirty;         // key -> picked, not yet written
bool flushQueued = false;

bool onGuiThread()
{
    return qApp && QThread::currentThread() == qApp->thread();
}

/*  Run f on the GUI thread: now if already there, queued otherwise. The FileOps
    notifications can come from a worker. */
template <typename F> void onGui(F f)
{
    if (onGuiThread()) f();
    else if (qApp) QMetaObject::invokeMethod(qApp, f, Qt::QueuedConnection);
}

/*  The keys of path itself and of its versions (path/#vN). The set is small -- a cull,
    not a catalog -- so a walk is fine. */
QStringList keysOf(const QString &path)
{
    QStringList out;
    const QString versions = path + VersionKey::kSep;
    for (const QString &k : std::as_const(keys))
        if (k == path || k.startsWith(versions)) out << k;
    return out;
}

}   // namespace

namespace SavedPicks {

void Open()
{
    if (opened) return;
    opened = true;
    keys.clear();
    dirty.clear();
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) {
        qWarning() << "SavedPicks::Open: picks will not be saved:"
                   << UserDb::instance().unavailableReason();
        return;
    }
    QSqlQuery q(d);
    q.setForwardOnly(true);
    if (!q.exec("SELECT key FROM saved_pick")) return;
    while (q.next()) keys.insert(q.value(0).toString());
}

bool IsOpen()
{
    return opened;
}

void Close()
{
    if (!opened) return;
    Flush();
    opened = false;
    keys.clear();
    dirty.clear();
}

const QSet<QString> &Keys()
{
    return keys;
}

bool Contains(const QString &key)
{
    return keys.contains(key);
}

int Count()
{
    return int(keys.size());
}

void Set(const QString &key, bool picked)
{
    if (!opened || key.isEmpty() || !onGuiThread()) return;
    if (picked == keys.contains(key)) return;
    if (picked) keys.insert(key);
    else keys.remove(key);
    dirty.insert(key, picked);
    /*  NEXT TURN OF THE EVENT LOOP, not a debounce: every toggle path changes its whole
        selection inside one call, so one turn is already the whole batch, and a longer
        wait would only widen the window a power loss could take. */
    if (!flushQueued) {
        flushQueued = true;
        QTimer::singleShot(0, qApp, [] { Flush(); });
    }
}

void Flush()
{
    flushQueued = false;
    if (dirty.isEmpty()) return;
    const QHash<QString, bool> batch = std::exchange(dirty, {});
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) return;                // in memory only (see Open)
    /*  One transaction: a failure part-way keeps the previous state, not half a
        multi-select. */
    if (!d.transaction()) return;
    QSqlQuery add(d), remove(d);
    add.prepare("INSERT OR IGNORE INTO saved_pick (key) VALUES (?)");
    remove.prepare("DELETE FROM saved_pick WHERE key = ?");
    for (auto it = batch.cbegin(); it != batch.cend(); ++it) {
        QSqlQuery &q = it.value() ? add : remove;
        q.addBindValue(it.key());
        if (!q.exec()) {
            qWarning() << "SavedPicks::Flush:" << q.lastError().text();
            d.rollback();
            return;
        }
    }
    if (!d.commit()) qWarning() << "SavedPicks::Flush:" << d.lastError().text();
}

void ClearAll()
{
    if (!opened) return;
    keys.clear();
    dirty.clear();
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) return;
    QSqlQuery q(d);
    if (!q.exec("DELETE FROM saved_pick"))
        qWarning() << "SavedPicks::ClearAll:" << q.lastError().text();
}

void OnMoved(const QString &srcPath, const QString &dstPath)
{
/*
    A move or rename Winnow made carries the pick, versions included. The datamodel
    re-key (DataModel::setData, KeyRole) makes the same change when the row is loaded;
    Set is idempotent, so whichever comes second does nothing.
*/
    onGui([srcPath, dstPath] {
        if (!opened || srcPath == dstPath) return;
        for (const QString &k : keysOf(srcPath)) {
            Set(k, false);
            Set(dstPath + k.mid(srcPath.size()), true);
        }
    });
}

void OnDeleted(const QString &path)
{
    onGui([path] {
        if (!opened) return;
        for (const QString &k : keysOf(path)) Set(k, false);
    });
}

void OnFolderDeleted(const QString &folder)
{
    onGui([folder] {
        if (!opened || folder.isEmpty()) return;
        const QString prefix = folder.endsWith('/') ? folder : folder + "/";
        QStringList gone;
        for (const QString &k : std::as_const(keys))
            if (k.startsWith(prefix)) gone << k;
        for (const QString &k : gone) Set(k, false);
    });
}

}   // namespace SavedPicks
