#include "Cache/devpreviewcache.h"
#include "Cache/cachedb.h"
#include "Cache/pathkey.h"
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

QString defaultCacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/PreviewCache";
}

/*
    A snapshot of the currently mounted volume roots.

    Taking one walks the whole mount table, which costs a statfs per volume. sweep() used
    to take a fresh one FOR EVERY ENTRY -- at the 250,000 entries this cache is sized for
    that is a quarter of a million mount-table walks per sweep. One snapshot per sweep
    turns the per-entry cost into a list lookup.

    Deliberately a value, not a cached singleton with a time-to-live. The verdict this
    feeds -- "the file is missing, so demote the entry" -- is only safe while the volume
    really is mounted, and a stale snapshot that still listed an ejected card would demote
    every preview on it. A caller takes a snapshot when it starts and lives with the
    window it already had.
*/
struct MountSnapshot
{
    static MountSnapshot take()
    {
        MountSnapshot m;
        const auto volumes = QStorageInfo::mountedVolumes();
        m.roots.reserve(volumes.size());
        for (const QStorageInfo &si : volumes) {
            if (!si.isValid() || !si.isReady()) continue;
            const QString root = QDir::fromNativeSeparators(si.rootPath());
            if (root.isEmpty()) continue;
            m.roots.append(root);
        }
        return m;
    }

    /* The LONGEST mounted root that prefixes path, so /Volumes/Photos/a.nef resolves to
       /Volumes/Photos and not to "/". */
    QString rootOf(const QString &path) const
    {
        QString best;
        const QString p = QDir::fromNativeSeparators(path);
        for (const QString &root : roots) {
            if (root.length() <= best.length()) continue;
            const QString withSep = root.endsWith('/') ? root : root + "/";
            if (p.startsWith(withSep, Qt::CaseInsensitive)) best = root;
        }
        return best;
    }

    /* An entry written before volRoot was recorded, or one on the boot volume, is treated
       as mounted -- the boot volume is always there. */
    bool isMounted(const QString &volRoot) const
    {
        if (volRoot.isEmpty()) return true;
        return roots.contains(volRoot);
    }

    QStringList roots;
};

/*
    What the file at path looks like right now: its length and last-modified time. Used to
    confirm that the image an entry was made from is still the image sitting at that path.
*/
struct SrcStamp
{
    qint64 size = 0;
    qint64 mtime = 0;
    bool valid = false;

    static SrcStamp of(const QString &path)
    {
        const QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) return SrcStamp();
        SrcStamp s;
        s.size = fi.size();
        s.mtime = fi.lastModified().toSecsSinceEpoch();
        s.valid = true;
        return s;
    }

    /* Does what is on disk CONTRADICT the entry -- i.e. is this a different image now?

       Nothing on disk at all is deliberately NOT a contradiction. A missing source is the
       sweep's demote case: the file is in the trash or its volume is ejected, the preview
       is still the right picture of it, and the cache exists partly so the loupe can
       still show it. Only a file that is present and DIFFERENT means the path has been
       reused, which is the case that would otherwise serve the wrong photograph.

       An entry with nothing recorded cannot contradict anything until a sweep stamps it.
    */
    bool contradicts(qint64 entrySize, qint64 entryMtime) const
    {
        if (!entrySize && !entryMtime) return false;
        if (!valid) return false;
        return size != entrySize || mtime != entryMtime;
    }
};

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
    if (!q.exec("SELECT folder, COUNT(*), SUM(live), SUM(bytes) FROM devpreview"
                " GROUP BY folder ORDER BY folder")) {
        return list;
    }
    while (q.next()) {
        FolderStat f;
        f.folder = q.value(0).toString();
        f.count = q.value(1).toInt();
        f.live = q.value(2).toInt();
        f.bytes = q.value(3).toLongLong();
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
              "  srcsize, srcmtime)"
              " VALUES (?, ?, ?, ?, ?, ?, ?, 1, ?, ?, ?)"
              " ON CONFLICT(pathkey) DO UPDATE SET"
              " id = excluded.id, path = excluded.path, folder = excluded.folder,"
              " hash = excluded.hash, bytes = excluded.bytes, used = excluded.used,"
              " live = 1, vol = excluded.vol, srcsize = excluded.srcsize,"
              " srcmtime = excluded.srcmtime");
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
    u.prepare("UPDATE devpreview SET used = ?, live = 1 WHERE id = ?");
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
              " live = 1, srcsize = ?, srcmtime = ? WHERE pathkey = ?");
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
    the same image.

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
    };

    int demoted = 0;
    quint64 cursor = 0;

    forever {
        QList<Row> page;
        {
            QMutexLocker lk(&mutex);
            QSqlDatabase db = dbLocked();
            if (!db.isOpen()) return demoted;
            QSqlQuery q(db);
            q.prepare("SELECT id, path, vol, live, srcsize, srcmtime, pathkey"
                      " FROM devpreview WHERE id > ? ORDER BY id LIMIT ?");
            q.addBindValue(qulonglong(cursor));
            q.addBindValue(kPageRows);
            if (!q.exec()) return demoted;
            while (q.next()) {
                page.append({q.value(0).toULongLong(), q.value(1).toString(),
                             q.value(2).toString(), q.value(3).toBool(),
                             q.value(4).toLongLong(), q.value(5).toLongLong(),
                             q.value(6).toString()});
            }
        }
        if (page.isEmpty()) break;
        cursor = page.last().id;

        /* Stat unlocked -- this is the slow part. */
        QList<quint64> toDemote;
        QList<quint64> toRevive;
        QList<QString> replaced;                 // keys whose path holds another image
        QList<QPair<quint64, SrcStamp>> toStamp;

        for (const Row &r : page) {
            /* Ejected card / unplugged drive: we know nothing about these files, so leave
               the row exactly as it is. */
            if (!mounts.isMounted(r.vol)) continue;

            const SrcStamp stamp = SrcStamp::of(r.path);
            if (!stamp.valid) {
                if (r.live) toDemote.append(r.id);   // demote, do not delete
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
            && toStamp.isEmpty()) {
            continue;
        }

        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return demoted;
        const bool inTxn = db.transaction();

        if (!toDemote.isEmpty()) {
            QSqlQuery q(db);
            q.prepare("UPDATE devpreview SET live = 0 WHERE id = ?");
            for (quint64 id : toDemote) {
                q.addBindValue(qulonglong(id));
                if (q.exec()) ++demoted;
            }
        }
        if (!toRevive.isEmpty()) {
            QSqlQuery q(db);
            q.prepare("UPDATE devpreview SET live = 1 WHERE id = ?");
            for (quint64 id : toRevive) {
                q.addBindValue(qulonglong(id));
                q.exec();
            }
        }
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

    if (demoted && G::isLogger)
        G::log("DevPreviewCache::sweep", "demoted " + QString::number(demoted));
    return demoted;
}

void DevPreviewCache::reconcile()
{
/*
    Make the table and the folder of payloads agree.

    ONE directory listing and ONE table scan, matched through a set of ids. The obvious
    shape -- stat the payload named by each row, then list the folder to find strays -- is
    two passes over 250,000 files and a quarter of a million stat calls; this is one pass
    over the names alone.

    A cache file with no row can never be attributed to an image again (the id in its name
    says nothing about which picture it came from), so it is dead weight. A row with no
    file is a row that can only ever miss.
*/
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;

    const QString d = dirLocked();

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

    /* Rows whose payload is gone, and the ids that are legitimately claimed. */
    QList<QString> lost;
    QSet<quint64> claimed;
    {
        QSqlQuery q(db);
        if (!q.exec("SELECT id, pathkey FROM devpreview")) return;
        while (q.next()) {
            const quint64 id = q.value(0).toULongLong();
            if (onDisk.contains(id)) claimed.insert(id);
            else lost.append(q.value(1).toString());
        }
    }

    const bool inTxn = db.transaction();
    for (const QString &p : lost) removeLocked(db, p);
    if (inTxn) db.commit();

    for (quint64 id : onDisk) {
        if (claimed.contains(id)) continue;
        QFile::remove(filePathLocked(id));
    }
    for (const QString &f : unnamed) QFile::remove(f);
}

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
