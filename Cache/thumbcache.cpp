#include "Cache/thumbcache.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSqlQuery>
#include <QVariant>

#include "Cache/cachedb.h"
#include "Cache/mountsnapshot.h"
#include "Cache/pathkey.h"

namespace {
/*  Sweep page size. The sweep stats one file per row, so it works in pages and
    releases the lock between them -- at 250,000 rows on a network volume,
    holding it throughout would block every get() for the duration, which is a
    frozen scroll. DevPreviewCache's sweep is paged for the same reason. */
constexpr int kPageRows = 2000;

qint64 nowSecs() { return QDateTime::currentSecsSinceEpoch(); }
}

ThumbCache &ThumbCache::instance()
{
    static ThumbCache c;
    return c;
}

void ThumbCache::setMaxBytes(qint64 bytes)
{
    QMutexLocker lk(&mMutex);
    mMaxBytes = bytes > 0 ? bytes : 0;
}

qint64 ThumbCache::maxBytes() const
{
    QMutexLocker lk(&mMutex);
    return mMaxBytes;
}

QSqlDatabase ThumbCache::dbLocked() const
{
    return CacheDb::instance().db();
}

void ThumbCache::put(const QString &fPath, const QByteArray &jpg, int w, int h,
                     qint64 srcSize, qint64 srcMtime)
{
    if (fPath.isEmpty() || jpg.isEmpty()) return;
    const QString key = cachePathKey(fPath);
    if (key.isEmpty()) return;

    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;              // no index: degrade to "no cache"

    const QFileInfo fi(fPath);
    QSqlQuery q(db);
    /*  One statement rather than a delete plus an insert: a row replaced while a
        reader is mid-SELECT would otherwise be briefly absent, and a scroll
        would show a gap where a thumbnail had always been. */
    q.prepare("INSERT INTO thumb (pathkey, path, folder, jpg, w, h, bytes, used,"
              " live, vol, srcsize, srcmtime)"
              " VALUES (?,?,?,?,?,?,?,?,1,?,?,?)"
              " ON CONFLICT(pathkey) DO UPDATE SET"
              " path=excluded.path, folder=excluded.folder, jpg=excluded.jpg,"
              " w=excluded.w, h=excluded.h, bytes=excluded.bytes,"
              " used=excluded.used, live=1, vol=excluded.vol,"
              " srcsize=excluded.srcsize, srcmtime=excluded.srcmtime");
    q.addBindValue(key);
    q.addBindValue(fPath);
    q.addBindValue(fi.absolutePath());
    q.addBindValue(jpg);
    q.addBindValue(w);
    q.addBindValue(h);
    q.addBindValue(qint64(jpg.size()));
    q.addBindValue(nowSecs());
    q.addBindValue(MountSnapshot::take().rootOf(fPath));
    q.addBindValue(srcSize);
    q.addBindValue(srcMtime);
    if (!q.exec()) return;

    evictLocked(db);
}

QByteArray ThumbCache::get(const QString &fPath, qint64 srcSize, qint64 srcMtime)
{
    const QString key = cachePathKey(fPath);
    if (key.isEmpty()) return QByteArray();

    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return QByteArray();

    QSqlQuery q(db);
    q.prepare("SELECT jpg, srcsize, srcmtime FROM thumb WHERE pathkey = ?");
    q.addBindValue(key);
    if (!q.exec() || !q.next()) return QByteArray();

    /*  STALENESS. The thumbnail is only valid for the bytes it was made from.
        A source whose size or mtime has moved is a MISS, not a stale picture --
        an image edited by another program keeps its path, and showing
        yesterday's picture for today's file is the one failure a cache must not
        have. A row stamped with zeros predates the stamp and is grandfathered,
        the way DevPreviewCache grandfathers its imported rows. */
    const qint64 rowSize = q.value(1).toLongLong();
    const qint64 rowMtime = q.value(2).toLongLong();
    if ((rowSize || rowMtime) && (rowSize != srcSize || rowMtime != srcMtime))
        return QByteArray();

    const QByteArray jpg = q.value(0).toByteArray();
    if (jpg.isEmpty()) return QByteArray();

    QSqlQuery u(db);
    u.prepare("UPDATE thumb SET used = ?, live = 1 WHERE pathkey = ?");
    u.addBindValue(nowSecs());
    u.addBindValue(key);
    u.exec();
    return jpg;
}

bool ThumbCache::contains(const QString &fPath, qint64 srcSize, qint64 srcMtime) const
{
    const QString key = cachePathKey(fPath);
    if (key.isEmpty()) return false;

    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("SELECT srcsize, srcmtime FROM thumb WHERE pathkey = ?");
    q.addBindValue(key);
    if (!q.exec() || !q.next()) return false;
    const qint64 rowSize = q.value(0).toLongLong();
    const qint64 rowMtime = q.value(1).toLongLong();
    if ((rowSize || rowMtime) && (rowSize != srcSize || rowMtime != srcMtime))
        return false;
    return true;
}

void ThumbCache::onMoved(const QString &srcPath, const QString &dstPath)
{
    const QString from = cachePathKey(srcPath);
    const QString to = cachePathKey(dstPath);
    if (from.isEmpty() || to.isEmpty()) return;

    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;
    /*  The destination may already have a row -- moving over an existing image.
        Drop that one first so the unique key is free; the thumbnail that
        travels with the file is the correct one. */
    QSqlQuery d(db);
    d.prepare("DELETE FROM thumb WHERE pathkey = ?");
    d.addBindValue(to);
    d.exec();

    QSqlQuery q(db);
    q.prepare("UPDATE thumb SET pathkey = ?, path = ?, folder = ?, vol = ?"
              " WHERE pathkey = ?");
    q.addBindValue(to);
    q.addBindValue(dstPath);
    q.addBindValue(QFileInfo(dstPath).absolutePath());
    q.addBindValue(MountSnapshot::take().rootOf(dstPath));
    q.addBindValue(from);
    q.exec();
}

void ThumbCache::onDeleted(const QString &fPath)
{
    const QString key = cachePathKey(fPath);
    if (key.isEmpty()) return;
    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("DELETE FROM thumb WHERE pathkey = ?");
    q.addBindValue(key);
    q.exec();
}

int ThumbCache::sweep()
{
/*
    DEMOTE, NEVER DELETE, AND SKIP WHAT IS NOT MOUNTED. A missing source file
    means the row is not worth keeping; it does NOT mean the image is gone. An
    ejected card, an unplugged drive or a network share that is not up would
    otherwise look identical to a deletion, and throwing away the thumbnails for
    a whole volume because it was not plugged in is exactly the behaviour that
    makes a cache feel unreliable. So a row whose volume is absent is skipped,
    and a row whose file is genuinely missing is marked live = 0, which puts it
    first in line for eviction without destroying it -- a file restored from the
    trash finds its thumbnail again.

    PAGED, with the lock released between pages: this stats one file per row,
    and holding the lock across 250,000 of those would block every get() for the
    duration. DevPreviewCache's sweep is paged for the same reason.
*/
    const MountSnapshot mounts = MountSnapshot::take();

    /*  PAGED BY rowid, NOT BY pathkey. The first attempt used the key as the
        cursor and swept nothing at all: a default-constructed QString is NULL to
        SQLite, and `pathkey > NULL` is NULL rather than true, so the very first
        page came back empty and the pass reported a clean sweep. The unit test
        caught it. rowid is an integer, starts usefully at 0, is the physical
        order so the scan is sequential, and is what DevPreviewCache's sweep
        already pages on. */
    struct Row { qint64 id; QString key; QString path; QString vol; bool live; };
    int demoted = 0;
    qint64 cursor = 0;

    forever {
        QList<Row> page;
        {
            QMutexLocker lk(&mMutex);
            QSqlDatabase db = dbLocked();
            if (!db.isOpen()) return demoted;
            QSqlQuery q(db);
            q.prepare("SELECT rowid, pathkey, path, vol, live FROM thumb"
                      " WHERE rowid > ? ORDER BY rowid LIMIT ?");
            q.addBindValue(cursor);
            q.addBindValue(kPageRows);
            if (!q.exec()) return demoted;
            while (q.next())
                page.append({q.value(0).toLongLong(), q.value(1).toString(),
                             q.value(2).toString(), q.value(3).toString(),
                             q.value(4).toBool()});
        }
        if (page.isEmpty()) break;
        cursor = page.last().id;

        /*  Stat unlocked -- this is the slow part. */
        QStringList toDemote, toRevive;
        for (const Row &r : page) {
            if (!mounts.isMounted(r.vol)) continue;      // ejected, not gone
            const bool exists = QFileInfo::exists(r.path);
            if (!exists && r.live) toDemote << r.key;
            else if (exists && !r.live) toRevive << r.key;
        }
        if (toDemote.isEmpty() && toRevive.isEmpty()) continue;

        QMutexLocker lk(&mMutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return demoted;
        for (const QString &k : toDemote) {
            QSqlQuery q(db);
            q.prepare("UPDATE thumb SET live = 0 WHERE pathkey = ?");
            q.addBindValue(k);
            if (q.exec()) ++demoted;
        }
        for (const QString &k : toRevive) {
            QSqlQuery q(db);
            q.prepare("UPDATE thumb SET live = 1 WHERE pathkey = ?");
            q.addBindValue(k);
            q.exec();
        }
    }
    return demoted;
}

void ThumbCache::evictLocked(QSqlDatabase &db)
{
/*
    Drop rows until the total is within the cap, DEMOTED FIRST and then least
    recently used -- which is what the (live, used) index is ordered on, so
    choosing what to drop is an index scan of only the rows actually being
    dropped rather than a sort of the whole table.
*/
    QSqlQuery t(db);
    if (!t.exec("SELECT COALESCE(SUM(bytes), 0) FROM thumb") || !t.next()) return;
    qint64 total = t.value(0).toLongLong();
    if (total <= mMaxBytes) return;

    forever {
        QSqlQuery q(db);
        q.prepare("SELECT pathkey, bytes FROM thumb ORDER BY live, used LIMIT ?");
        q.addBindValue(kPageRows);
        if (!q.exec()) return;
        QStringList keys;
        QList<qint64> sizes;
        while (q.next()) { keys << q.value(0).toString(); sizes << q.value(1).toLongLong(); }
        if (keys.isEmpty()) return;

        for (int i = 0; i < keys.size() && total > mMaxBytes; ++i) {
            QSqlQuery d(db);
            d.prepare("DELETE FROM thumb WHERE pathkey = ?");
            d.addBindValue(keys.at(i));
            if (d.exec()) total -= sizes.at(i);
        }
        if (total <= mMaxBytes) return;
        if (keys.size() < kPageRows) return;        // nothing left to drop
    }
}

void ThumbCache::clear()
{
    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.exec("DELETE FROM thumb");
}

int ThumbCache::count() const
{
    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    if (!q.exec("SELECT COUNT(*) FROM thumb") || !q.next()) return 0;
    return q.value(0).toInt();
}

qint64 ThumbCache::totalBytes() const
{
    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    if (!q.exec("SELECT COALESCE(SUM(bytes), 0) FROM thumb") || !q.next()) return 0;
    return q.value(0).toLongLong();
}
