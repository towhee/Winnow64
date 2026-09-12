#include "Cache/devpreviewcache.h"
#include "Cache/cachedb.h"
#include "Cache/pathkey.h"
#include "Cache/mountsnapshot.h"
#include "Main/global.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStorageInfo>
#include <algorithm>

namespace {

const char *kDbName = "index.db";
const char *kJsonName = "index.json";    // the index this replaced; imported once

/* How many rows a long pass handles between takes of the mutex. Big enough that the
   locking is not the cost, small enough that a get() on the GUI thread never waits on
   more than a few hundred stats. */
constexpr int kPageRows = 512;

/* How long a demoted row keeps its payload before the sweep reaps it.

   Demotion exists so that a source file restored from the trash finds its preview again
   rather than paying for a re-render, and thirty days is comfortably longer than anyone
   takes to notice a deletion they did not mean. Past that the row is holding several MB
   for an image that is not coming back. A constant rather than a preference: the right
   value does not vary by user, and a preview is always re-renderable, so there is
   nothing here for anyone to tune. */
constexpr qint64 kDemotedGraceSecs = 30LL * 24 * 60 * 60;

QString defaultCacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/PreviewCache";
}

qint64 nowSecs()
{
    return QDateTime::currentSecsSinceEpoch();
}

}  // namespace

DevPreviewCache &DevPreviewCache::instance()
{
    static DevPreviewCache cache;
    return cache;
}

/* ---------------------------------------------------------------------------------
   Location and capacity
   --------------------------------------------------------------------------------- */

QString DevPreviewCache::dirLocked() const
{
    return dir.isEmpty() ? defaultCacheDir() : dir;
}

void DevPreviewCache::setCacheDir(const QString &d)
{
/*
    Point the cache at a directory. Deliberately does NOT open the index here: lazy open
    on first use is the ONLY load path, so there is exactly one way for it to arrive and
    no way for a caller to skip it. The app never calls this at all -- the default
    location is used and the first put/get opens it.
*/
    QMutexLocker lk(&mutex);
    if (dir == d && loaded) return;
    bytes = 0;
    nextId = 1;
    loaded = false;
    dir = d;
    CacheDb::instance().closeThisThread();
}

QString DevPreviewCache::cacheDir() const
{
    QMutexLocker lk(&mutex);
    return dirLocked();
}

void DevPreviewCache::setMaxBytes(qint64 b)
{
    QMutexLocker lk(&mutex);
    capBytes = qMax(0LL, b);
    QSqlDatabase db = dbLocked();
    if (db.isOpen()) evictLocked(db);
}

bool DevPreviewCache::isCachePath(const QString &path) const
{
/*
    Is path the cache folder or something inside it? Compared case-insensitively: the two
    supported platforms both have case-insensitive filesystems by default, and a folder
    reached as ".../previewcache" must be protected exactly like ".../PreviewCache".

    absoluteFilePath rather than canonicalFilePath, because the cache folder need not
    exist yet (nothing has been developed) and canonicalFilePath returns empty for a path
    that does not resolve, which would silently protect nothing.
*/
    if (path.isEmpty()) return false;

    QString d;
    {
        QMutexLocker lk(&mutex);
        d = dirLocked();
    }
    d = QDir::cleanPath(QFileInfo(d).absoluteFilePath());
    if (d.isEmpty()) return false;

    const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (!p.compare(d, Qt::CaseInsensitive)) return true;
    return p.startsWith(d + "/", Qt::CaseInsensitive);
}

QString DevPreviewCache::readOnlyReason()
{
    return "the develop preview cache can only be viewed, not edited";
}

qint64 DevPreviewCache::maxBytes() const
{
    QMutexLocker lk(&mutex);
    return capBytes;
}

qint64 DevPreviewCache::totalBytes() const
{
    QMutexLocker lk(&mutex);
    const_cast<DevPreviewCache *>(this)->ensureLoadedLocked();
    return bytes;
}

int DevPreviewCache::count() const
{
    QMutexLocker lk(&mutex);
    QSqlDatabase db = const_cast<DevPreviewCache *>(this)->dbLocked();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    if (!q.exec("SELECT COUNT(*) FROM devpreview") || !q.next()) return 0;
    return q.value(0).toInt();
}

QList<DevPreviewCache::FolderStat> DevPreviewCache::folderStats() const
{
/*
    Grouped by the stored folder column rather than by splitting every path in memory --
    at 250,000 rows the difference is a scan of an index against building a quarter of a
    million QFileInfos.
*/
    QList<FolderStat> list;
    QMutexLocker lk(&mutex);
    QSqlDatabase db = const_cast<DevPreviewCache *>(this)->dbLocked();
    if (!db.isOpen()) return list;

    QSqlQuery q(db);
    if (!q.exec("SELECT folder, COUNT(*), SUM(live), SUM(bytes), MIN(vol) FROM devpreview"
                " GROUP BY folder ORDER BY folder")) {
        return list;
    }
    /*  MOUNTED IS REPORTED BECAUSE live DOES NOT IMPLY IT. The sweep leaves a row on an
        unmounted volume exactly as it found it -- we know nothing about those files, and
        treating an ejected card as a mass deletion is the one verdict this whole design
        avoids -- so those rows read live, and the "missing source images" note against
        the demoted count understates them. A folder is one directory, so one volume:
        MIN(vol) is that volume, not an arbitrary pick. */
    const MountSnapshot mounts = MountSnapshot::take();
    while (q.next()) {
        FolderStat f;
        f.folder = q.value(0).toString();
        f.count = q.value(1).toInt();
        f.live = q.value(2).toInt();
        f.bytes = q.value(3).toLongLong();
        f.mounted = mounts.isMounted(q.value(4).toString());
        list.append(f);
    }
    return list;
}

QString DevPreviewCache::filePathLocked(quint64 id) const
{
    return dirLocked() + "/" + QString::number(id, 16) + ".jpg";
}

/* ---------------------------------------------------------------------------------
   Volume awareness

   The sweep must never mistake an ejected card or an unplugged external drive for a
   folder full of deleted images. Each entry records the mount point it was created
   under, and the sweep only trusts a "file is missing" verdict when that mount point is
   currently mounted and ready.

   The work lives in MountSnapshot above. These two are the one-shot convenience wrappers
   for the callers that look at a single path; anything that loops takes its own snapshot
   and keeps it.
   --------------------------------------------------------------------------------- */

QString DevPreviewCache::volumeRootOf(const QString &path)
{
    return MountSnapshot::take().rootOf(path);
}

bool DevPreviewCache::volumeMounted(const QString &volRoot)
{
    if (volRoot.isEmpty()) return true;
    return MountSnapshot::take().isMounted(volRoot);
}

/* ---------------------------------------------------------------------------------
   Core store
   --------------------------------------------------------------------------------- */

void DevPreviewCache::put(const QString &fPath, const QByteArray &blobHash,
                          const QByteArray &jpg)
{
/*
    THE PAYLOAD IS WRITTEN WITHOUT THE MUTEX HELD. At full sensor resolution this is a
    multi-MB QSaveFile write plus a commit, and put() runs once per image through a
    background preview build -- holding the one cache mutex across that stalls every
    decoder thread in get(), which is the opposite of what this cache is for. The lock is
    taken twice instead: once to reserve the id, once to record the row.
*/
    if (fPath.isEmpty() || blobHash.isEmpty() || jpg.isEmpty()) return;

    const QString key = cachePathKey(fPath);

    quint64 id = 0;
    QString payloadPath;
    {
        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return;
        if (capBytes <= 0) return;

        QDir().mkpath(dirLocked());

        /* Reuse the id of any existing row for this image so we overwrite one file rather
           than leaking the old one. */
        QSqlQuery q(db);
        q.prepare("SELECT id FROM devpreview WHERE pathkey = ?");
        q.addBindValue(key);
        if (q.exec() && q.next()) id = q.value(0).toULongLong();
        if (!id) id = nextId++;
        payloadPath = filePathLocked(id);
    }

    QSaveFile f(payloadPath);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(jpg);
    if (!f.commit()) return;

    /* Stamp the source image, so a later occupant of this path cannot inherit this
       preview. See STALENESS in the header. */
    const SrcStamp stamp = SrcStamp::of(fPath);

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;

    /* What the row said BEFORE this write. Normally the id we just reused, in which case
       there is nothing to clean up; different only if another put for the same image
       landed while we were writing, and then one of the two payloads is now orphaned. */
    qint64 oldBytes = 0;
    quint64 priorId = 0;
    {
        QSqlQuery q(db);
        q.prepare("SELECT id, bytes FROM devpreview WHERE pathkey = ?");
        q.addBindValue(key);
        if (q.exec() && q.next()) {
            priorId = q.value(0).toULongLong();
            oldBytes = q.value(1).toLongLong();
        }
    }

    QSqlQuery q(db);
    q.prepare("INSERT INTO devpreview"
              " (id, path, pathkey, folder, hash, bytes, used, live, vol,"
              "  srcsize, srcmtime, demoted)"
              " VALUES (?, ?, ?, ?, ?, ?, ?, 1, ?, ?, ?, 0)"
              " ON CONFLICT(pathkey) DO UPDATE SET"
              " id = excluded.id, path = excluded.path, folder = excluded.folder,"
              " hash = excluded.hash, bytes = excluded.bytes, used = excluded.used,"
              " live = 1, vol = excluded.vol, srcsize = excluded.srcsize,"
              " srcmtime = excluded.srcmtime, demoted = 0");
    q.addBindValue(qulonglong(id));
    q.addBindValue(fPath);
    q.addBindValue(key);
    q.addBindValue(QFileInfo(fPath).absolutePath());
    q.addBindValue(QString::fromLatin1(blobHash));
    q.addBindValue(qint64(jpg.size()));
    q.addBindValue(nowSecs());
    q.addBindValue(volumeRootOf(fPath));
    q.addBindValue(stamp.size);
    q.addBindValue(stamp.mtime);
    if (!q.exec()) {
        /* The payload is on disk with no row naming it. reconcile() will collect it. */
        QFile::remove(payloadPath);
        return;
    }

    if (priorId && priorId != id) QFile::remove(filePathLocked(priorId));

    bytes += qint64(jpg.size()) - oldBytes;
    evictLocked(db);
}

QByteArray DevPreviewCache::get(const QString &fPath, const QByteArray &blobHash)
{
/*
    THE PAYLOAD IS READ WITHOUT THE MUTEX HELD, for the reason put() writes without it: at
    full sensor resolution this is a multi-MB read, and every one of ImageCache's
    decoderCount threads comes through here. Serialising them on one mutex would undo the
    parallel read-ahead that makes browsing developed raws fast.

    Releasing the lock opens one window: the row can be evicted, or replaced by a put,
    between the lookup and the read. Both resolve safely. An evicted payload fails to open
    and answers with a miss, which is what a miss looks like anyway; a replaced one is
    caught by re-checking the id before doing anything destructive.
*/
    const QString key = cachePathKey(fPath);

    quint64 id = 0;
    QString payloadPath;
    {
        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return QByteArray();

        QSqlQuery q(db);
        q.prepare("SELECT id, hash, srcsize, srcmtime FROM devpreview WHERE pathkey = ?");
        q.addBindValue(key);
        if (!q.exec() || !q.next()) return QByteArray();
        if (q.value(1).toString().toLatin1() != blobHash) return QByteArray();

        /* Is the file at this path still the image this preview depicts? The recipe hash
           cannot answer that -- a preset gives a whole folder the same hash -- so the
           source stamp does. */
        if (SrcStamp::of(fPath).contradicts(q.value(2).toLongLong(),
                                            q.value(3).toLongLong())) {
            removeLocked(db, key);
            return QByteArray();
        }

        id = q.value(0).toULongLong();
        payloadPath = filePathLocked(id);
    }

    QFile f(payloadPath);
    if (!f.open(QIODevice::ReadOnly)) {
        /* The payload is gone. Drop the row so we stop counting its bytes -- but only if
           it is still the row we looked at. A put that landed while we were unlocked has
           already replaced it with a payload that does exist. */
        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return QByteArray();
        QSqlQuery q(db);
        q.prepare("SELECT id FROM devpreview WHERE pathkey = ?");
        q.addBindValue(key);
        if (q.exec() && q.next() && q.value(0).toULongLong() == id)
            removeLocked(db, key);
        return QByteArray();
    }
    const QByteArray jpg = f.readAll();

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return jpg;       // the pixels are good; only the touch is lost
    QSqlQuery u(db);
    u.prepare("UPDATE devpreview SET used = ?, live = 1, demoted = 0 WHERE id = ?");
    u.addBindValue(nowSecs());
    u.addBindValue(qulonglong(id));
    u.exec();

    return jpg;
}

bool DevPreviewCache::contains(const QString &fPath, const QByteArray &blobHash) const
{
    QMutexLocker lk(&mutex);
    QSqlDatabase db = const_cast<DevPreviewCache *>(this)->dbLocked();
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("SELECT hash, srcsize, srcmtime FROM devpreview WHERE pathkey = ?");
    q.addBindValue(cachePathKey(fPath));
    if (!q.exec() || !q.next()) return false;
    if (q.value(0).toString().toLatin1() != blobHash) return false;
    /* Same source-identity check as get(), minus the repair -- contains() is const, and a
       caller asking "is there one?" must get the same answer get() would give. */
    return !SrcStamp::of(fPath).contradicts(q.value(1).toLongLong(),
                                            q.value(2).toLongLong());
}

QString DevPreviewCache::payloadPath(const QString &fPath) const
{
/*
    See the header: where this image's payload lives, for the Develop diagnostics. No
    recipe hash and no LRU touch -- this reports the mapping, not a cache hit.
*/
    if (fPath.isEmpty()) return QString();
    QMutexLocker lk(&mutex);
    auto *self = const_cast<DevPreviewCache *>(this);
    QSqlDatabase db = self->dbLocked();
    if (!db.isOpen()) return QString();

    QSqlQuery q(db);
    q.prepare("SELECT id FROM devpreview WHERE pathkey = ?");
    q.addBindValue(cachePathKey(fPath));
    if (!q.exec() || !q.next()) return QString();
    return self->filePathLocked(q.value(0).toULongLong());
}

void DevPreviewCache::removeLocked(QSqlDatabase &db, const QString &key)
{
/*
    Takes the NORMALISED key (cachePathKey), not a path -- every caller here already has
    one, and taking a path would invite a caller to pass a raw string that matches
    nothing.
*/
    quint64 id = 0;
    qint64 b = 0;
    {
        QSqlQuery q(db);
        q.prepare("SELECT id, bytes FROM devpreview WHERE pathkey = ?");
        q.addBindValue(key);
        if (!q.exec() || !q.next()) return;
        id = q.value(0).toULongLong();
        b = q.value(1).toLongLong();
    }

    QSqlQuery del(db);
    del.prepare("DELETE FROM devpreview WHERE pathkey = ?");
    del.addBindValue(key);
    if (!del.exec()) return;

    QFile::remove(filePathLocked(id));
    bytes -= b;
}

void DevPreviewCache::evictLocked(QSqlDatabase &db)
{
/*
    Evict until we are inside the cap. Demoted entries (source file missing at the last
    sweep) go first, then genuine LRU -- which is exactly the devpreview_evict index, so
    choosing what to drop reads only the rows being dropped rather than sorting the whole
    table. That matters: once the cache is full, EVERY put evicts.
*/
    if (bytes <= capBytes) return;

    forever {
        if (bytes <= capBytes) break;

        struct Doomed { quint64 id; qint64 bytes; };
        QList<Doomed> doomed;
        qint64 planned = 0;
        {
            QSqlQuery q(db);
            q.prepare("SELECT id, bytes FROM devpreview"
                      " ORDER BY live ASC, used ASC, id ASC LIMIT ?");
            q.addBindValue(kPageRows);
            if (!q.exec()) return;
            while (q.next()) {
                doomed.append({q.value(0).toULongLong(), q.value(1).toLongLong()});
                planned += doomed.last().bytes;
                if (bytes - planned <= capBytes) break;
            }
        }
        if (doomed.isEmpty()) break;      // nothing left to give

        qint64 freed = 0;
        const bool inTxn = db.transaction();
        {
            QSqlQuery del(db);
            del.prepare("DELETE FROM devpreview WHERE id = ?");
            for (const Doomed &d : doomed) {
                del.addBindValue(qulonglong(d.id));
                if (!del.exec()) continue;
                QFile::remove(filePathLocked(d.id));
                freed += d.bytes;
            }
        }
        if (inTxn && !db.commit()) return;

        if (freed <= 0) break;            // deletes are failing; do not spin
        bytes -= freed;
    }
}

/* ---------------------------------------------------------------------------------
   File-operation sync (see Utilities/fileops.h)
   --------------------------------------------------------------------------------- */

void DevPreviewCache::onMoved(const QString &srcPath, const QString &dstPath)
{
/*
    Committed here and now, not at shutdown. A move that lived only in memory was lost to
    a crash, and what survived was an index still naming the SOURCE path -- which is not
    merely a forgotten preview. Let something else take that path later and the row
    describes a different image; if the two share a recipe, as every image given the same
    preset does, the hash agrees too. The source stamp is the backstop for a rename Winnow
    never saw; this is the fix for the ones it did.
*/
    if (srcPath.isEmpty() || dstPath.isEmpty() || srcPath == dstPath) return;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;

    const QString srcKey = cachePathKey(srcPath);
    const QString dstKey = cachePathKey(dstPath);
    if (srcKey == dstKey) return;           // a spelling change, not a move

    {
        QSqlQuery q(db);
        q.prepare("SELECT id FROM devpreview WHERE pathkey = ?");
        q.addBindValue(srcKey);
        if (!q.exec() || !q.next()) return;      // nothing cached for the source
    }

    /* A pre-existing entry at the destination is being overwritten by this move, so its
       file has to go or it leaks -- and its row has to go or the unique key collides. */
    removeLocked(db, dstKey);

    /* Same bytes, same image, new home. Re-stamp so the row describes the file actually
       at dstPath: a copy carries its own mtime. */
    const SrcStamp stamp = SrcStamp::of(dstPath);

    QSqlQuery q(db);
    q.prepare("UPDATE devpreview SET path = ?, pathkey = ?, folder = ?, vol = ?,"
              " live = 1, demoted = 0, srcsize = ?, srcmtime = ? WHERE pathkey = ?");
    q.addBindValue(dstPath);
    q.addBindValue(dstKey);
    q.addBindValue(QFileInfo(dstPath).absolutePath());
    q.addBindValue(volumeRootOf(dstPath));
    q.addBindValue(stamp.size);
    q.addBindValue(stamp.mtime);
    q.addBindValue(srcKey);
    q.exec();
}

void DevPreviewCache::onDeleted(const QString &fPath)
{
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;
    removeLocked(db, cachePathKey(fPath));
}

void DevPreviewCache::clear()
{
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.exec("DELETE FROM devpreview");
    }
    bytes = 0;

    /* Sweep up the payloads, including anything the index did not know about. */
    QDir cd(dirLocked());
    const auto strays = cd.entryList(QStringList() << "*.jpg", QDir::Files);
    for (const QString &s : strays) QFile::remove(cd.filePath(s));
}

/* ---------------------------------------------------------------------------------
   Orphan sweep
   --------------------------------------------------------------------------------- */

int DevPreviewCache::sweep()
{
/*
    Walk every row, confirming that the image it was made from is still there and still
    the same image, and collect the ones that have been gone long enough.

    DEMOTE, THEN REAP. A missing source demotes the row rather than deleting it, so a
    file restored from the trash finds its preview again. That patience used to have no
    end: a demoted row kept its multi-MB payload until the LRU cap was breached, which
    on a cache well under its cap is never. One real cache was half dead camera-card
    entries on that account. The demoted column records WHEN, and a row still missing a
    grace period later goes for good -- row and payload together.

    PAGED, AND THE MUTEX IS RELEASED BETWEEN PAGES. This stats one file per row; at
    250,000 rows on a network volume, holding the lock throughout would block every get()
    for the duration, which is a frozen loupe. Rows are taken in id order so the cursor
    survives the gaps, and a row inserted or deleted while the sweep runs is simply seen
    or not seen -- both are correct, and the pass is idempotent.
*/
    /* ONE mount-table walk for the whole sweep -- see MountSnapshot. */
    const MountSnapshot mounts = MountSnapshot::take();

    struct Row {
        quint64 id;
        QString path;
        QString vol;
        bool live;
        qint64 srcSize;
        qint64 srcMtime;
        QString key;
        qint64 demotedAt;
    };

    int demoted = 0;
    int reaped = 0;
    quint64 cursor = 0;
    const qint64 now = nowSecs();

    forever {
        QList<Row> page;
        {
            QMutexLocker lk(&mutex);
            QSqlDatabase db = dbLocked();
            if (!db.isOpen()) return demoted;
            QSqlQuery q(db);
            q.prepare("SELECT id, path, vol, live, srcsize, srcmtime, pathkey, demoted"
                      " FROM devpreview WHERE id > ? ORDER BY id LIMIT ?");
            q.addBindValue(qulonglong(cursor));
            q.addBindValue(kPageRows);
            if (!q.exec()) return demoted;
            while (q.next()) {
                page.append({q.value(0).toULongLong(), q.value(1).toString(),
                             q.value(2).toString(), q.value(3).toBool(),
                             q.value(4).toLongLong(), q.value(5).toLongLong(),
                             q.value(6).toString(), q.value(7).toLongLong()});
            }
        }
        if (page.isEmpty()) break;
        cursor = page.last().id;

        /* Stat unlocked -- this is the slow part. */
        QList<quint64> toDemote;
        QList<quint64> toRevive;
        QList<QString> replaced;                 // keys whose path holds another image
        QList<QPair<quint64, SrcStamp>> toStamp;
        QList<quint64> toDate;                   // demoted before the column existed
        QList<QString> toReap;                   // demoted longer than the grace period

        /*  THE REAP CLOCK RUNS ON A ROW THAT IS ALREADY DEMOTED, MOUNTED OR NOT. Demotion
            is a verdict already reached, back when the volume WAS mounted and the file
            WAS missing; nothing about ejecting the card makes that less true, and
            requiring the volume to be present again would mean a card that is never
            reinserted keeps its previews forever -- which is precisely the population
            this reap exists to collect (one real cache held 5 GB of exactly that). The
            mount check below still guards the verdict itself: a LIVE row on an unmounted
            volume is left strictly alone, because "path does not exist" there means
            "ejected", not "deleted". */
        auto ageDemoted = [&](const Row &r) {
            /*  A row demoted before the column existed reads 0, which is "we do not know
                when", not "long ago": stamp it now and reap it a grace period from here.
                Same treatment, and the same reason, as the srcsize / srcmtime backfill
                below. */
            if (!r.demotedAt) toDate.append(r.id);
            else if (r.demotedAt < now - kDemotedGraceSecs) toReap.append(r.key);
        };

        for (const Row &r : page) {
            if (!mounts.isMounted(r.vol)) {
                if (!r.live) ageDemoted(r);
                continue;
            }

            const SrcStamp stamp = SrcStamp::of(r.path);
            if (!stamp.valid) {
                /*  Demote, do not delete -- a file that comes back from the trash finds
                    its preview intact. But that patience has an end: the row kept its
                    multi-MB payload until the LRU cap was breached, which on a cache
                    well under its cap is forever. */
                if (r.live) { toDemote.append(r.id); continue; }
                ageDemoted(r);
                continue;
            }
            if (!r.live) toRevive.append(r.id);      // came back (restored from trash)

            /* The sweep is the one pass that already stats every source image, so it is
               where the stamp is maintained. A row with none is one imported from the
               JSON index, which predates it: stamp it now and it is verified from here
               on. A row whose stamp DISAGREES describes something that is no longer the
               image it was made from, so it goes. */
            if (!r.srcSize && !r.srcMtime) toStamp.append({r.id, stamp});
            else if (stamp.contradicts(r.srcSize, r.srcMtime)) replaced.append(r.key);
        }

        if (toDemote.isEmpty() && toRevive.isEmpty() && replaced.isEmpty()
            && toStamp.isEmpty() && toDate.isEmpty() && toReap.isEmpty()) {
            continue;
        }

        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return demoted;
        const bool inTxn = db.transaction();

        if (!toDemote.isEmpty()) {
            /* live and demoted move together: the timestamp is what the reap above
               reads, and a demoted row without one would never be collected. */
            QSqlQuery q(db);
            q.prepare("UPDATE devpreview SET live = 0, demoted = ? WHERE id = ?");
            for (quint64 id : toDemote) {
                q.addBindValue(now);
                q.addBindValue(qulonglong(id));
                if (q.exec()) ++demoted;
            }
        }
        if (!toRevive.isEmpty()) {
            QSqlQuery q(db);
            q.prepare("UPDATE devpreview SET live = 1, demoted = 0 WHERE id = ?");
            for (quint64 id : toRevive) {
                q.addBindValue(qulonglong(id));
                q.exec();
            }
        }
        if (!toDate.isEmpty()) {
            QSqlQuery q(db);
            q.prepare("UPDATE devpreview SET demoted = ? WHERE id = ?");
            for (quint64 id : toDate) {
                q.addBindValue(now);
                q.addBindValue(qulonglong(id));
                q.exec();
            }
        }
        /* removeLocked unlinks the payload and decrements bytes, so a reap is the one
           place a demoted entry actually gives its disk back. */
        for (const QString &k : toReap) { removeLocked(db, k); ++reaped; }
        if (!toStamp.isEmpty()) {
            QSqlQuery q(db);
            q.prepare("UPDATE devpreview SET srcsize = ?, srcmtime = ? WHERE id = ?");
            for (const auto &s : toStamp) {
                q.addBindValue(s.second.size);
                q.addBindValue(s.second.mtime);
                q.addBindValue(qulonglong(s.first));
                q.exec();
            }
        }
        for (const QString &p : replaced) removeLocked(db, p);

        if (inTxn) db.commit();
    }

    lastReapedCount.storeRelaxed(reaped);
    if ((demoted || reaped) && G::isLogger)
        G::log("DevPreviewCache::sweep", "demoted " + QString::number(demoted) +
                                             ", reaped " + QString::number(reaped));
    return demoted;
}

void DevPreviewCache::reconcile()
{
/*
    Make the table and the folder of payloads agree.

    WHY IT MATTERS AT ALL. A payload with no row can never be attributed to an image
    again -- the id in its name says nothing about which picture it came from -- and,
    worse, it is invisible to the byte cap, which sums the TABLE. So a stray is disk the
    cache does not know it is holding and can never reclaim. Two paths create them
    deliberately: the schema-2 pathkey dedupe (Cache/cachedb.cpp), and CacheDb::moveAside,
    which renames an unreadable index and starts a fresh one -- stranding the entire
    payload folder at a stroke. A row with no file is the mirror image: it can only ever
    miss, and it inflates the byte total so the cap evicts live entries early.

    ONE directory listing and ONE table scan, matched through a set of ids. The obvious
    shape -- stat the payload named by each row, then list the folder to find strays -- is
    two passes over 250,000 files and a quarter of a million stat calls; this is one pass
    over the names alone.

    THE MUTEX IS NOT HELD ACROSS THE I/O. It used to be held for the whole pass, which at
    250,000 entries blocks every decoder thread's get() for seconds -- a frozen loupe --
    and contradicted this class's own rule that the long passes work in pages. The
    listing and every unlink now run unlocked, and the table is read a page at a time.

    EVERY DELETION IS RE-CHECKED UNDER THE LOCK, and that is what makes the unlocked I/O
    safe. put() writes its payload with the mutex DROPPED and takes it again to insert the
    row, so a put() straddling this pass can put a file on disk that the listing missed
    and a row in the table the scan missed -- in either order. Set arithmetic over two
    stale snapshots would then read "payload with no row" or "row with no payload" and
    delete something perfectly good. So the snapshots only ever nominate CANDIDATES:
    before a row goes, its payload is stat'd again; before a file goes, the table is asked
    again for its id. Candidates are rare, so the re-check costs a handful of queries
    rather than a second pass. (An id watermark was tried instead and is wrong: after a
    moveAside the fresh table starts at id 1, so every stray on disk outranks it and the
    one case this exists for would be skipped forever.)
*/
    QString d;
    {
        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();       // also runs the one-time JSON import
        if (!db.isOpen()) return;
        d = dirLocked();
    }
    auto payloadFor = [&d](quint64 id) {
        return d + "/" + QString::number(id, 16) + ".jpg";
    };

    /* Ids present on disk. QDirIterator reads names only -- no stat per file. */
    QSet<quint64> onDisk;
    QList<QString> unnamed;                  // *.jpg whose name is not an id at all
    QDirIterator it(d, QStringList() << "*.jpg", QDir::Files);
    while (it.hasNext()) {
        const QString file = it.next();
        bool ok = false;
        const quint64 id = QFileInfo(file).completeBaseName().toULongLong(&ok, 16);
        if (ok) onDisk.insert(id);
        else unnamed.append(file);
    }

    /* Rows whose payload is gone, and the ids that are legitimately claimed. Paged in id
       order like sweep(), so the cursor survives rows appearing and disappearing. */
    QSet<quint64> claimed;
    int lostRows = 0;
    quint64 cursor = 0;
    forever {
        struct Row { quint64 id; QString key; };
        QList<Row> page;
        {
            QMutexLocker lk(&mutex);
            QSqlDatabase db = dbLocked();
            if (!db.isOpen()) return;
            QSqlQuery q(db);
            q.prepare("SELECT id, pathkey FROM devpreview WHERE id > ? ORDER BY id"
                      " LIMIT ?");
            q.addBindValue(qulonglong(cursor));
            q.addBindValue(kPageRows);
            if (!q.exec()) return;
            while (q.next())
                page.append({q.value(0).toULongLong(), q.value(1).toString()});
        }
        if (page.isEmpty()) break;
        cursor = page.last().id;

        QList<QString> lost;
        for (const Row &r : page) {
            if (onDisk.contains(r.id)) { claimed.insert(r.id); continue; }
            /* Candidate only. Unlocked stat: the listing may predate a put() that has
               since written this very file. */
            if (!QFileInfo::exists(payloadFor(r.id))) lost.append(r.key);
            else claimed.insert(r.id);
        }
        if (lost.isEmpty()) continue;

        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return;
        const bool inTxn = db.transaction();
        for (const QString &k : lost) { removeLocked(db, k); ++lostRows; }
        if (inTxn) db.commit();
    }

    /* The files nothing claims. Asked of the table again, under the lock, because a row
       inserted after this pass read its page is not in `claimed` and its payload is not a
       stray. */
    QList<quint64> candidates;
    for (quint64 id : onDisk)
        if (!claimed.contains(id)) candidates.append(id);

    int strays = 0;
    for (int i = 0; i < candidates.count(); i += kPageRows) {
        const int n = qMin(kPageRows, candidates.count() - i);
        QList<quint64> confirmed;
        {
            QMutexLocker lk(&mutex);
            QSqlDatabase db = dbLocked();
            if (!db.isOpen()) break;
            QSqlQuery q(db);
            q.prepare("SELECT 1 FROM devpreview WHERE id = ?");
            for (int j = i; j < i + n; ++j) {
                q.addBindValue(qulonglong(candidates.at(j)));
                if (q.exec() && q.next()) continue;   // claimed after all
                confirmed.append(candidates.at(j));
            }
        }
        for (quint64 id : confirmed)
            if (QFile::remove(payloadFor(id))) ++strays;
    }

    /* A name that is not a hex id was not written by this cache and no row can ever
       name it. The folder is browsable and every write path refuses it (isCachePath), so
       anything here arrived by hand, against that policy. */
    for (const QString &f : unnamed)
        if (QFile::remove(f)) ++strays;

    lastStrayCount.storeRelaxed(strays);
    lastLostRowCount.storeRelaxed(lostRows);
    if ((strays || lostRows) && G::isLogger)
        G::log("DevPreviewCache::reconcile", "strays " + QString::number(strays) +
                                                 ", lost rows " + QString::number(lostRows));
}

int DevPreviewCache::lastReaped() const   { return lastReapedCount.loadRelaxed(); }
int DevPreviewCache::lastStrays() const   { return lastStrayCount.loadRelaxed(); }
int DevPreviewCache::lastLostRows() const { return lastLostRowCount.loadRelaxed(); }

/* ---------------------------------------------------------------------------------
   Opening and housekeeping
   --------------------------------------------------------------------------------- */

void DevPreviewCache::ensureLoadedLocked()
{
    if (loaded) return;
    loaded = true;                  // set first: a failure must not retry on every call
    bytes = 0;
    nextId = 1;

    const QString d = dirLocked();
    QDir().mkpath(d);
    CacheDb::instance().setPath(d + "/" + QString::fromLatin1(kDbName));

    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return;       // no cache this session; every get misses

    migrateJsonIndexLocked(db);

    /* The two pieces of state that are NOT re-read per query. nextId comes from MAX(id)
       rather than a stored counter, so it cannot restart at 1 and clobber the payload
       belonging to another image -- the failure that made the old lazy-load bug
       destructive rather than merely wasteful. */
    QSqlQuery q(db);
    if (q.exec("SELECT COALESCE(SUM(bytes), 0), COALESCE(MAX(id), 0) FROM devpreview")
        && q.next()) {
        bytes = q.value(0).toLongLong();
        nextId = q.value(1).toULongLong() + 1;
    }
}

QSqlDatabase DevPreviewCache::dbLocked()
{
    ensureLoadedLocked();
    return CacheDb::instance().db();
}

void DevPreviewCache::migrateJsonIndexLocked(QSqlDatabase &db)
{
/*
    Import the JSON index this class used to keep, once, on the first open of a database
    that does not have the rows yet. Without this every existing cache would read as empty
    on the upgrade and reconcile() would then delete the lot -- minutes of re-rendering
    per folder, for a change that is supposed to be invisible.

    The JSON is RENAMED rather than deleted, so a migration that goes wrong is recoverable
    by hand. It carries no source stamp (it predates one); those rows import with zero and
    the first sweep stamps them.
*/
    const QString jsonPath = dirLocked() + "/" + QString::fromLatin1(kJsonName);
    if (!QFileInfo::exists(jsonPath)) return;

    {
        QSqlQuery q(db);
        if (!q.exec("SELECT COUNT(*) FROM devpreview") || !q.next()) return;
        if (q.value(0).toInt() > 0) {
            QFile::rename(jsonPath, jsonPath + ".migrated");   // already done
            return;
        }
    }

    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QString msg = "Preview cache index unreadable; starting a new cache.";
        G::issue("Warning", msg, "DevPreviewCache::migrateJsonIndex", -1, jsonPath);
        QFile::rename(jsonPath, jsonPath + ".unreadable");
        return;
    }

    const QJsonArray arr = doc.object().value("entries").toArray();
    const bool inTxn = db.transaction();
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO devpreview"
              " (id, path, pathkey, folder, hash, bytes, used, live, vol,"
              "  srcsize, srcmtime)"
              " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    int imported = 0;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        const QString path = o.value("path").toString();
        const quint64 id = quint64(o.value("id").toDouble(0));
        if (path.isEmpty() || id == 0) continue;
        q.addBindValue(qulonglong(id));
        q.addBindValue(path);
        q.addBindValue(cachePathKey(path));
        q.addBindValue(QFileInfo(path).absolutePath());
        q.addBindValue(o.value("hash").toString());
        q.addBindValue(qint64(o.value("bytes").toDouble(0)));
        q.addBindValue(qint64(o.value("used").toDouble(0)));
        q.addBindValue(o.value("live").toBool(true) ? 1 : 0);
        q.addBindValue(o.value("vol").toString());
        q.addBindValue(qint64(o.value("ssz").toDouble(0)));
        q.addBindValue(qint64(o.value("smt").toDouble(0)));
        if (q.exec()) ++imported;
    }
    if (inTxn && !db.commit()) return;

    QFile::rename(jsonPath, jsonPath + ".migrated");
    if (G::isLogger)
        G::log("DevPreviewCache::migrateJsonIndex",
               "imported " + QString::number(imported));
}

void DevPreviewCache::load()
{
    QMutexLocker lk(&mutex);
    loaded = false;
    ensureLoadedLocked();
}

void DevPreviewCache::save()
{
/*
    Every mutation is committed when it happens, so there is nothing to flush. What this
    does is fold the write-ahead log back into the database, which keeps the -wal file
    from growing across a long session. The name is kept because MW::closeEvent and the
    startup sweep both call it.
*/
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.exec("PRAGMA wal_checkpoint(TRUNCATE)");
}
