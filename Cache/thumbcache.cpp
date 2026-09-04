#include "Cache/thumbcache.h"

#include <QBuffer>
#include <QElapsedTimer>
#include <QTimer>
#include <QDateTime>
#include <QFileInfo>
#include <QSqlQuery>
#include <QVariant>

#include "Cache/cachedb.h"
#include "Main/global.h"
#include "Cache/mountsnapshot.h"
#include "Cache/pathkey.h"

namespace {
/*  Sweep page size. The sweep stats one file per row, so it works in pages and
    releases the lock between them -- at 250,000 rows on a network volume,
    holding it throughout would block every get() for the duration, which is a
    frozen scroll. DevPreviewCache's sweep is paged for the same reason. */
constexpr int kPageRows = 2000;

/*  How stale the LRU `used` stamp may get before a read refreshes it. See get():
    refreshing it on EVERY read makes a cache hit cost a WAL commit, and used only
    orders eviction -- a day's granularity is all that ordering needs. */
constexpr qint64 kUsedStampMaxAgeSecs = 24 * 60 * 60;

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
    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;              // no index: degrade to "no cache"
    putLocked(db, fPath, jpg, w, h, srcSize, srcMtime);
    evictLocked(db);
}

void ThumbCache::putLocked(QSqlDatabase &db, const QString &fPath, const QByteArray &jpg,
                           int w, int h, qint64 srcSize, qint64 srcMtime)
{
    if (fPath.isEmpty() || jpg.isEmpty()) return;
    const QString key = cachePathKey(fPath);
    if (key.isEmpty()) return;

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
    q.exec();
}


/*  THE WRITER. One thread, batched transactions -- see putImage in the header
    for the measurement that made this necessary rather than optional, and for
    why the handoff is a queued signal rather than a queue of our own.
*/
namespace { constexpr int kInFlightMax = 256; constexpr int kBatch = 64; }

void ThumbWriter::take(const QString &fPath, const QImage &im)
{
    /*  Worker thread. mPending exists only here, so there is nothing to lock. */
    mPending.append({fPath, im});
    if (mPending.size() >= kBatch) { flushPending(); return; }
    if (!mScheduled) {
        mScheduled = true;
        /*  A zero-timer, so a burst of arrivals collects into one transaction
            and a trickle still lands as soon as the thread is idle. */
        QTimer::singleShot(0, this, &ThumbWriter::flushPending);
    }
}

void ThumbWriter::takeStamp(const QString &key)
{
    /*  Worker thread, like take(). Duplicates within a batch are harmless -- the same
        UPDATE twice in one transaction is one write -- so they are not filtered here. */
    mPendingStamps.append(key);
    if (mPendingStamps.size() >= kBatch) { flushPending(); return; }
    if (!mScheduled) {
        mScheduled = true;
        QTimer::singleShot(0, this, &ThumbWriter::flushPending);
    }
}

void ThumbWriter::flushPending()
{
    mScheduled = false;
    if (!mPendingStamps.isEmpty()) {
        const QStringList stamps = mPendingStamps;
        mPendingStamps.clear();
        mOwner->writeStampBatch(stamps);
    }
    if (mPending.isEmpty()) return;
    const QList<QPair<QString, QImage>> batch = mPending;
    mPending.clear();
    mOwner->writeBatch(batch);
    mOwner->mInFlight.fetchAndAddRelaxed(-batch.size());
}

void ThumbCache::writeBatch(const QList<QPair<QString, QImage>> &batch)
{
/*
    ONE TRANSACTION FOR THE WHOLE BATCH. Committing per row is what made the
    inline version cost 14 ms an icon: in WAL every statement outside an explicit
    transaction is its own commit, and several reader threads were queueing for
    one writer. Sixty-four rows in one commit is the same work in one commit.
*/
    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;

    const bool inTxn = db.transaction();
    for (const auto &job : batch) {
        const QFileInfo fi(job.first);
        if (!fi.exists()) continue;
        const qint64 srcSize = fi.size();
        const qint64 srcMtime = fi.lastModified().toSecsSinceEpoch();

        /*  SKIP IF THE INDEX ALREADY HAS THIS ONE, checked here rather than at
            the producer so a revisit costs the loader nothing at all. Without
            it every revisit re-encodes and rewrites the whole folder, which is
            the opposite of what the cache is for. */
        if (containsLocked(db, job.first, srcSize, srcMtime)) continue;

        /*  Quality 85 at thumbnail size is visually indistinguishable from 100
            and roughly half the bytes; the icon is a browsing aid, never a
            source for anything. */
        QByteArray jpg;
        QBuffer buf(&jpg);
        if (!buf.open(QIODevice::WriteOnly)) continue;
        if (!job.second.save(&buf, "JPG", 85)) continue;
        buf.close();
        if (jpg.isEmpty()) continue;

        putLocked(db, job.first, jpg, job.second.width(), job.second.height(),
                  srcSize, srcMtime);
        mWritten.fetchAndAddRelaxed(1);
    }
    if (inTxn) db.commit();

    evictLocked(db);
}

void ThumbCache::startWriterLocked()
{
    if (mWriter) return;
    mThread = new QThread;
    mThread->setObjectName("ThumbWriter");
    mWriter = new ThumbWriter(this);
    mWriter->moveToThread(mThread);
    /*  The connection belongs to the thread that opened it, so it is closed on
        the way out rather than left for whoever tears the thread down. */
    QObject::connect(mThread, &QThread::finished, mWriter, [] {
        CacheDb::instance().closeThisThread();
    });
    QObject::connect(mThread, &QThread::finished, mWriter, &QObject::deleteLater);
    mThread->start(QThread::LowPriority);
}

void ThumbCache::putImage(const QString &fPath, const QImage &im, bool hasDevelopRecipe)
{
    /*  wantsOriginalThumb: for an edited image in a developed-showing mode the
        picture in hand is the DEVELOPED thumbnail, and storing that would serve
        an edit in Original mode later. See the declaration. */
    if (!G::cacheThumbnails || !wantsOriginalThumb(hasDevelopRecipe)) return;
    if (fPath.isEmpty() || im.isNull()) return;

    ThumbWriter *w;
    {
        QMutexLocker lk(&mMutex);
        startWriterLocked();
        w = mWriter;
    }
    /*  Bounded: drop rather than grow. See mInFlight in the header. */
    if (mInFlight.loadRelaxed() >= kInFlightMax) return;
    mInFlight.fetchAndAddRelaxed(1);
    QMetaObject::invokeMethod(w, "take", Qt::QueuedConnection,
                              Q_ARG(QString, fPath), Q_ARG(QImage, im));
}

void ThumbCache::flush()
{
    ThumbWriter *w;
    {
        QMutexLocker lk(&mMutex);
        w = mWriter;
    }
    if (!w) return;
    /*  A BLOCKING invoke, which is the whole trick: it cannot run until every
        queued take() ahead of it has, so when it returns the backlog is
        written. No condition variable, no busy-wait, and nothing shared. */
    QMetaObject::invokeMethod(w, "flushPending", Qt::BlockingQueuedConnection);
}

ThumbCache::~ThumbCache()
{
    QThread *t = nullptr;
    {
        QMutexLocker lk(&mMutex);
        t = mThread;
        mThread = nullptr;
        mWriter = nullptr;
    }
    if (t) { t->quit(); t->wait(); delete t; }
}

QByteArray ThumbCache::get(const QString &fPath, qint64 srcSize, qint64 srcMtime)
{
    const QString key = cachePathKey(fPath);
    if (key.isEmpty()) return QByteArray();

    /*  Probe only: the wait for mMutex is separated from the work done under it,
        because EVERY Reader thread serialises here and a hit that looks expensive may
        only be a hit that queued. */
    QElapsedTimer gTimer;
    if (G::isPerfProbe) gTimer.start();
    QMutexLocker lk(&mMutex);
    if (G::isPerfProbe) {
        G::probeThumbLockNs.fetch_add(gTimer.nsecsElapsed(), std::memory_order_relaxed);
        gTimer.restart();
    }

    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return QByteArray();

    QSqlQuery q(db);
    q.prepare("SELECT jpg, srcsize, srcmtime, used FROM thumb WHERE pathkey = ?");
    q.addBindValue(key);
    const bool got = q.exec() && q.next();
    if (G::isPerfProbe)
        G::probeThumbSqlNs.fetch_add(gTimer.nsecsElapsed(), std::memory_order_relaxed);
    if (!got) return QByteArray();

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

    /*  THE LRU STAMP IS NOT REFRESHED ON EVERY READ, and that is the difference
        between a cache hit costing a SELECT and costing a COMMIT.

        This UPDATE is a WRITE on the READ path, and in WAL every statement outside
        an explicit transaction is its own commit -- the exact cost writeBatch()
        exists to avoid, reintroduced where nobody was looking for it. It is also
        under mMutex, so every Reader thread serialises behind it. MEASURED on 1,048
        raws with the cache warm: 1,034 hits accumulating 283 s of thread time across
        the reader pool, ~274 ms of lock wait per hit, and phase 2 at 25 s wall for a
        folder whose thumbnails were all already stored. It was ~95% of the load.

        used ORDERS EVICTION; IT IS NOT CORRECTNESS. A day's granularity keeps the
        ordering it is actually used for (evictLocked drops the least recently used
        when the cache exceeds its byte budget, and nothing distinguishes two
        thumbnails read minutes apart) while making the overwhelmingly common case --
        a thumbnail read again in the same session, or the same day -- a pure read.
        live = 1 rides along with it: a row that is being served is by definition
        present, and if it were marked dead the sweep that did so would have found
        the file gone. */
    /*  AND IT IS NOT WRITTEN HERE AT ALL ANY MORE. A day's granularity made the
        refresh rare per ROW, but a folder load reads hundreds of rows whose stamp is a
        day old, and each of those was an UPDATE under this mutex -- in WAL, its own
        commit -- while every other Reader thread queued behind it. MEASURED on an idle
        machine, scrolling a warm cache: 10.2 ms of lock wait per hit against 0.21 ms of
        SELECT under the lock. Handed to the writer thread instead, which batches them
        into one transaction the way it already does for the thumbnails themselves. */
    const qint64 rowUsed = q.value(3).toLongLong();
    if (nowSecs() - rowUsed >= kUsedStampMaxAgeSecs) {
        startWriterLocked();
        if (mWriter) {
            if (G::isPerfProbe)
                G::probeThumbStamps.fetch_add(1, std::memory_order_relaxed);
            QMetaObject::invokeMethod(mWriter, "takeStamp", Qt::QueuedConnection,
                                      Q_ARG(QString, key));
        }
    }
    return jpg;
}

void ThumbCache::writeStampBatch(const QStringList &keys)
{
/*
    ONE TRANSACTION FOR THE WHOLE BATCH, the same shape as writeBatch and for the same
    reason: in WAL every statement outside an explicit transaction is its own commit.

    live = 1 rides along as it did on the read path: a row that was served is by
    definition present, and if it were marked dead the sweep that did so would have found
    the file gone.
*/
    if (keys.isEmpty()) return;
    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;

    const qint64 now = nowSecs();
    const bool inTxn = db.transaction();
    for (const QString &k : keys) {
        QSqlQuery u(db);
        u.prepare("UPDATE thumb SET used = ?, live = 1 WHERE pathkey = ?");
        u.addBindValue(now);
        u.addBindValue(k);
        u.exec();
    }
    if (inTxn) db.commit();
}

bool ThumbCache::wantsOriginalThumb(bool hasDevelopRecipe)
{
    /*  No recipe, no developed thumbnail to prefer -- Thumb::devThumb finds
        nothing in the sidecar and falls through to the camera's picture, which
        is what this cache holds. This is the common case at every setting. */
    if (!hasDevelopRecipe) return true;

    /*  It has one, so the mode decides. The same condition Thumb::devThumb uses
        to claim the answer, read the other way round: Develop mode always shows
        developed -- you cannot edit what you cannot see -- regardless of the
        setting. */
    return G::operationMode != G::OperationMode::Develop
           && G::previewSource != G::PreviewSource::Developed;
}

QImage ThumbCache::getImage(const QString &fPath, bool hasDevelopRecipe,
                            qint64 srcSize, qint64 srcMtime)
{
    if (!G::cacheThumbnails || !wantsOriginalThumb(hasDevelopRecipe)) return QImage();
    if (fPath.isEmpty()) return QImage();

    /*  THE CALLER'S STAT, WHEN IT HAS ONE. A metadata+icon read has just stat'd this
        file -- Metadata::loadImageMetadata takes m.size from its QFileInfo, and the
        index path stats for IndexMetadata::candidate -- and a QFileInfo caches, so
        those two values cost the Reader nothing and are microseconds old. Doing it
        again here was a second syscall per row of a folder load: 73 us an icon on a
        quiet local scroll, and a far worse number on a network volume, for an answer
        already in hand. An icon-only read (scrolling into a chunk whose metadata is
        already loaded) has no such stat and passes -1, so it stats here as before.
        Probe only: the timer measures whichever of the two happened. */
    QElapsedTimer tcTimer;
    if (G::isPerfProbe) tcTimer.start();
    if (srcSize < 0 || srcMtime < 0) {
        const QFileInfo fi(fPath);
        if (!fi.exists()) {
            if (G::isPerfProbe)
                G::probeThumbStatNs.fetch_add(tcTimer.nsecsElapsed(),
                                              std::memory_order_relaxed);
            return QImage();
        }
        srcSize = fi.size();
        srcMtime = fi.lastModified().toSecsSinceEpoch();
    }
    if (G::isPerfProbe) {
        G::probeThumbStatNs.fetch_add(tcTimer.nsecsElapsed(), std::memory_order_relaxed);
        tcTimer.restart();
    }

    const QByteArray jpg = get(fPath, srcSize, srcMtime);
    if (jpg.isEmpty()) return QImage();

    if (G::isPerfProbe) tcTimer.restart();
    QImage im;
    if (!im.loadFromData(jpg, "JPG") || im.isNull()) return QImage();

    /*  Stored at G::maxIconSize, but a row written by an older build or under a
        larger icon setting must not paint over its cell -- the same guard
        Thumb::devThumb applies to a sidecar preview, for the same reason. */
    if (im.width() > G::maxIconSize || im.height() > G::maxIconSize)
        im = im.scaled(G::maxIconSize, G::maxIconSize,
                       Qt::KeepAspectRatio, Qt::FastTransformation);
    im.convertTo(QImage::Format_RGB32);
    if (G::isPerfProbe)
        G::probeThumbDecodeNs.fetch_add(tcTimer.nsecsElapsed(), std::memory_order_relaxed);
    return im;
}

bool ThumbCache::contains(const QString &fPath, qint64 srcSize, qint64 srcMtime) const
{
    QMutexLocker lk(&mMutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return false;
    return containsLocked(db, fPath, srcSize, srcMtime);
}

bool ThumbCache::containsLocked(QSqlDatabase &db, const QString &fPath,
                                qint64 srcSize, qint64 srcMtime) const
{
    const QString key = cachePathKey(fPath);
    if (key.isEmpty()) return false;
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
