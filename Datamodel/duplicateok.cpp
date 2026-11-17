#include "Datamodel/duplicateok.h"
#include "Datamodel/userdb.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QTimer>
#include <QVariant>

/*  See the header. Deliberately SavedPicks' code with the table renamed: the two stores
    answer the same question shape ("is this image in the user's set?") and should fail
    and recover the same way. */

namespace {

bool opened = false;
QSet<QString> paths;                // what is okayed, as far as the app is concerned
QHash<QString, bool> dirty;         // path -> okayed, not yet written
bool flushQueued = false;

bool onGuiThread()
{
    return qApp && QThread::currentThread() == qApp->thread();
}

template <typename F> void onGui(F f)
{
    if (onGuiThread()) f();
    else if (qApp) QMetaObject::invokeMethod(qApp, f, Qt::QueuedConnection);
}

}   // namespace

namespace DuplicateOk {

void Open()
{
    if (opened) return;
    opened = true;
    paths.clear();
    dirty.clear();
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) {
        qWarning() << "DuplicateOk::Open: okayed duplicates will not be saved:"
                   << UserDb::instance().unavailableReason();
        return;
    }
    QSqlQuery q(d);
    q.setForwardOnly(true);
    if (!q.exec("SELECT path FROM duplicate_ok")) return;
    while (q.next()) paths.insert(q.value(0).toString());
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
    paths.clear();
    dirty.clear();
}

const QSet<QString> &Paths()
{
    return paths;
}

bool IsOk(const QString &path)
{
    return paths.contains(path);
}

int Count()
{
    return int(paths.size());
}

void Set(const QString &path, bool ok)
{
    if (!opened || path.isEmpty() || !onGuiThread()) return;
    if (ok == paths.contains(path)) return;
    if (ok) paths.insert(path);
    else paths.remove(path);
    dirty.insert(path, ok);
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
    if (!d.transaction()) return;
    QSqlQuery add(d), remove(d);
    add.prepare("INSERT OR IGNORE INTO duplicate_ok (path) VALUES (?)");
    remove.prepare("DELETE FROM duplicate_ok WHERE path = ?");
    for (auto it = batch.cbegin(); it != batch.cend(); ++it) {
        QSqlQuery &q = it.value() ? add : remove;
        q.addBindValue(it.key());
        if (!q.exec()) {
            qWarning() << "DuplicateOk::Flush:" << q.lastError().text();
            d.rollback();
            return;
        }
    }
    if (!d.commit()) qWarning() << "DuplicateOk::Flush:" << d.lastError().text();
}

void OnMoved(const QString &srcPath, const QString &dstPath)
{
    onGui([srcPath, dstPath] {
        if (!opened || srcPath == dstPath || !paths.contains(srcPath)) return;
        Set(srcPath, false);
        Set(dstPath, true);
    });
}

void OnDeleted(const QString &path)
{
    onGui([path] {
        if (!opened) return;
        Set(path, false);
    });
}

void OnFolderDeleted(const QString &folder)
{
    onGui([folder] {
        if (!opened || folder.isEmpty()) return;
        const QString prefix = folder.endsWith('/') ? folder : folder + "/";
        QStringList gone;
        for (const QString &p : std::as_const(paths))
            if (p.startsWith(prefix)) gone << p;
        for (const QString &p : gone) Set(p, false);
    });
}

void OnFolderMoved(const QString &from, const QString &to)
{
    onGui([from, to] {
        if (!opened || from.isEmpty() || to.isEmpty() || from == to) return;
        const QString prefix = from.endsWith('/') ? from : from + "/";
        QStringList moving;
        for (const QString &p : std::as_const(paths))
            if (p.startsWith(prefix)) moving << p;
        for (const QString &p : std::as_const(moving)) {
            Set(p, false);
            Set(to + p.mid(prefix.size() - 1), true);
        }
    });
}

}   // namespace DuplicateOk
