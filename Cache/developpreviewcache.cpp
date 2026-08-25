#include "Cache/developpreviewcache.h"
#include "Main/global.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <algorithm>

namespace {

constexpr int kIndexVersion = 1;
const char *kIndexName = "index.json";

QString defaultCacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/PreviewCache";
}

}  // namespace

DevelopPreviewCache &DevelopPreviewCache::instance()
{
    static DevelopPreviewCache cache;
    return cache;
}

/* ---------------------------------------------------------------------------------
   Location and capacity
   --------------------------------------------------------------------------------- */

void DevelopPreviewCache::setCacheDir(const QString &d)
{
/*
    Point the cache at a directory. Deliberately does NOT read the index here: lazy load
    on first use is the ONLY load path, so there is exactly one way for the index to
    arrive and no way for a caller to skip it. The app never calls this at all -- the
    default location is used and the first put/get loads it.
*/
    QMutexLocker lk(&mutex);
    if (dir == d && loaded) return;
    entries.clear();
    bytes = 0;
    nextId = 1;
    dirty = false;
    loaded = false;
    dir = d;
}

QString DevelopPreviewCache::cacheDir() const
{
    QMutexLocker lk(&mutex);
    return dir.isEmpty() ? defaultCacheDir() : dir;
}

void DevelopPreviewCache::setMaxBytes(qint64 b)
{
    QMutexLocker lk(&mutex);
    capBytes = qMax(0LL, b);
    evictLocked();
}

qint64 DevelopPreviewCache::maxBytes() const
{
    QMutexLocker lk(&mutex);
    return capBytes;
}

qint64 DevelopPreviewCache::totalBytes() const
{
    QMutexLocker lk(&mutex);
    const_cast<DevelopPreviewCache*>(this)->ensureLoadedLocked();
    return bytes;
}

int DevelopPreviewCache::count() const
{
    QMutexLocker lk(&mutex);
    const_cast<DevelopPreviewCache*>(this)->ensureLoadedLocked();
    return entries.count();
}

QString DevelopPreviewCache::filePathLocked(quint64 id) const
{
    const QString d = dir.isEmpty() ? defaultCacheDir() : dir;
    return d + "/" + QString::number(id, 16) + ".jpg";
}

/* ---------------------------------------------------------------------------------
   Volume awareness

   The sweep must never mistake an ejected card or an unplugged external drive for a
   folder full of deleted images. Each entry records the mount point it was created
   under, and the sweep only trusts a "file is missing" verdict when that mount point
   is currently mounted and ready.

   volumeRootOf finds the LONGEST mounted root that prefixes the path, so
   /Volumes/Photos/a.nef resolves to /Volumes/Photos and not to "/".
   --------------------------------------------------------------------------------- */

QString DevelopPreviewCache::volumeRootOf(const QString &path)
{
    QString best;
    const QString p = QDir::fromNativeSeparators(path);
    const auto volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &si : volumes) {
        if (!si.isValid() || !si.isReady()) continue;
        QString root = QDir::fromNativeSeparators(si.rootPath());
        if (root.isEmpty()) continue;
        QString withSep = root.endsWith('/') ? root : root + "/";
        if (p.startsWith(withSep, Qt::CaseInsensitive) && root.length() > best.length())
            best = root;
    }
    return best;
}

bool DevelopPreviewCache::volumeMounted(const QString &volRoot)
{
    /* An entry written before volRoot was recorded, or one on the boot volume, is
       treated as mounted -- the boot volume is always there. */
    if (volRoot.isEmpty()) return true;
    const auto volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &si : volumes) {
        if (!si.isValid() || !si.isReady()) continue;
        if (QDir::fromNativeSeparators(si.rootPath()) == volRoot) return true;
    }
    return false;
}

/* ---------------------------------------------------------------------------------
   Core store
   --------------------------------------------------------------------------------- */

void DevelopPreviewCache::put(const QString &fPath, const QByteArray &blobHash,
                              const QByteArray &jpg)
{
    if (fPath.isEmpty() || blobHash.isEmpty() || jpg.isEmpty()) return;

    QMutexLocker lk(&mutex);
    ensureLoadedLocked();
    if (capBytes <= 0) return;

    const QString d = dir.isEmpty() ? defaultCacheDir() : dir;
    QDir().mkpath(d);

    /* Reuse the id of any existing entry for this path so we overwrite one file rather
       than leaking the old one. */
    quint64 id = 0;
    auto it = entries.find(fPath);
    if (it != entries.end()) {
        id = it->id;
        bytes -= it->bytes;
    }
    else {
        id = nextId++;
    }

    QSaveFile f(filePathLocked(id));
    if (!f.open(QIODevice::WriteOnly)) {
        if (it != entries.end()) bytes += it->bytes;   // undo the subtraction
        return;
    }
    f.write(jpg);
    if (!f.commit()) {
        if (it != entries.end()) bytes += it->bytes;
        return;
    }

    Entry e;
    e.id = id;
    e.blobHash = blobHash;
    e.bytes = jpg.size();
    e.lastUsed = QDateTime::currentSecsSinceEpoch();
    e.live = true;
    e.volRoot = volumeRootOf(fPath);
    entries.insert(fPath, e);
    bytes += e.bytes;
    dirty = true;

    evictLocked();
}

QByteArray DevelopPreviewCache::get(const QString &fPath, const QByteArray &blobHash)
{
    QMutexLocker lk(&mutex);
    ensureLoadedLocked();
    auto it = entries.find(fPath);
    if (it == entries.end()) return QByteArray();
    if (it->blobHash != blobHash) return QByteArray();   // recipe moved on

    QFile f(filePathLocked(it->id));
    if (!f.open(QIODevice::ReadOnly)) {
        /* The file vanished under us. Drop the entry so we stop counting its bytes. */
        bytes -= it->bytes;
        entries.erase(it);
        dirty = true;
        return QByteArray();
    }
    const QByteArray jpg = f.readAll();
    it->lastUsed = QDateTime::currentSecsSinceEpoch();
    it->live = true;
    dirty = true;
    return jpg;
}

bool DevelopPreviewCache::contains(const QString &fPath, const QByteArray &blobHash) const
{
    QMutexLocker lk(&mutex);
    const_cast<DevelopPreviewCache*>(this)->ensureLoadedLocked();
    auto it = entries.constFind(fPath);
    return it != entries.constEnd() && it->blobHash == blobHash;
}

void DevelopPreviewCache::removeLocked(const QString &fPath)
{
    auto it = entries.find(fPath);
    if (it == entries.end()) return;
    QFile::remove(filePathLocked(it->id));
    bytes -= it->bytes;
    entries.erase(it);
    dirty = true;
}

void DevelopPreviewCache::touchLocked(const QString &fPath)
{
    auto it = entries.find(fPath);
    if (it == entries.end()) return;
    it->lastUsed = QDateTime::currentSecsSinceEpoch();
    dirty = true;
}

/* Evict until we are inside the cap. Demoted entries (source file missing at the last
   sweep) go first, then genuine LRU. */
void DevelopPreviewCache::evictLocked()
{
    if (bytes <= capBytes) return;

    QList<QString> order = entries.keys();
    std::sort(order.begin(), order.end(),
              [this](const QString &a, const QString &b) {
                  const Entry &ea = entries[a];
                  const Entry &eb = entries[b];
                  if (ea.live != eb.live) return !ea.live;    // demoted first
                  return ea.lastUsed < eb.lastUsed;           // then oldest
              });

    for (const QString &p : order) {
        if (bytes <= capBytes) break;
        removeLocked(p);
    }
}

/* ---------------------------------------------------------------------------------
   File-operation sync (see Utilities/fileops.h)
   --------------------------------------------------------------------------------- */

void DevelopPreviewCache::onMoved(const QString &srcPath, const QString &dstPath)
{
    if (srcPath.isEmpty() || dstPath.isEmpty() || srcPath == dstPath) return;

    QMutexLocker lk(&mutex);
    ensureLoadedLocked();
    auto it = entries.find(srcPath);
    if (it == entries.end()) return;

    Entry e = *it;
    entries.erase(it);

    /* A pre-existing entry at the destination is being overwritten by this move, so its
       file has to go or it leaks. */
    auto old = entries.find(dstPath);
    if (old != entries.end()) {
        QFile::remove(filePathLocked(old->id));
        bytes -= old->bytes;
        entries.erase(old);
    }

    e.live = true;
    e.volRoot = volumeRootOf(dstPath);
    entries.insert(dstPath, e);
    dirty = true;
}

void DevelopPreviewCache::onDeleted(const QString &fPath)
{
    QMutexLocker lk(&mutex);
    ensureLoadedLocked();
    removeLocked(fPath);
}

void DevelopPreviewCache::clear()
{
    QMutexLocker lk(&mutex);
    ensureLoadedLocked();
    const QString d = dir.isEmpty() ? defaultCacheDir() : dir;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it)
        QFile::remove(filePathLocked(it->id));
    entries.clear();
    bytes = 0;
    dirty = true;

    /* Sweep up anything the index did not know about. */
    QDir cd(d);
    const auto strays = cd.entryList(QStringList() << "*.jpg", QDir::Files);
    for (const QString &s : strays) QFile::remove(cd.filePath(s));
}

/* ---------------------------------------------------------------------------------
   Orphan sweep
   --------------------------------------------------------------------------------- */

int DevelopPreviewCache::sweep()
{
    QMutexLocker lk(&mutex);
    ensureLoadedLocked();
    int demoted = 0;
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        /* Ejected card / unplugged drive: we know nothing about these files, so leave
           the entry exactly as it is. */
        if (!volumeMounted(it->volRoot)) continue;

        const bool exists = QFileInfo::exists(it.key());
        if (!exists && it->live) {
            it->live = false;                 // demote, do not delete
            ++demoted;
            dirty = true;
        }
        else if (exists && !it->live) {
            it->live = true;                  // came back (restored from trash)
            dirty = true;
        }
    }
    if (demoted && G::isLogger)
        G::log("DevelopPreviewCache::sweep", "demoted " + QString::number(demoted));
    return demoted;
}

void DevelopPreviewCache::reconcile()
{
    QMutexLocker lk(&mutex);
    ensureLoadedLocked();
    reconcileLocked();
}

void DevelopPreviewCache::reconcileLocked()
{
    const QString d = dir.isEmpty() ? defaultCacheDir() : dir;

    /* Index entries whose file is gone. */
    QList<QString> lost;
    QSet<QString> known;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QString fp = filePathLocked(it->id);
        if (QFileInfo::exists(fp)) known.insert(QFileInfo(fp).fileName());
        else lost.append(it.key());
    }
    for (const QString &p : lost) {
        auto it = entries.find(p);
        if (it == entries.end()) continue;
        bytes -= it->bytes;
        entries.erase(it);
        dirty = true;
    }

    /* Cache files with no index entry. The id in the filename cannot tell us which image
       it belonged to, so an unreferenced file is simply dead weight. */
    QDir cd(d);
    const auto files = cd.entryList(QStringList() << "*.jpg", QDir::Files);
    for (const QString &f : files) {
        if (known.contains(f)) continue;
        QFile::remove(cd.filePath(f));
    }
}

/* ---------------------------------------------------------------------------------
   Index persistence

   The index is the cache: without it a <id>.jpg cannot be attributed to any image. So a
   corrupt or unreadable index is not recoverable by scanning -- it means starting over,
   which costs nothing but a re-render.
   --------------------------------------------------------------------------------- */

void DevelopPreviewCache::ensureLoadedLocked()
{
    if (loaded) return;
    loadLocked();
    reconcileLocked();
}

void DevelopPreviewCache::load()
{
    QMutexLocker lk(&mutex);
    loadLocked();
    reconcileLocked();
}

void DevelopPreviewCache::loadLocked()
{
    {
        const QString d = dir.isEmpty() ? defaultCacheDir() : dir;
        loaded = true;
        entries.clear();
        bytes = 0;
        nextId = 1;

        QFile f(d + "/" + kIndexName);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            f.close();
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonObject root = doc.object();
                if (root.value("version").toInt() <= kIndexVersion) {
                    nextId = quint64(root.value("nextId").toDouble(1));
                    const QJsonArray arr = root.value("entries").toArray();
                    for (const QJsonValue &v : arr) {
                        const QJsonObject o = v.toObject();
                        const QString path = o.value("path").toString();
                        if (path.isEmpty()) continue;
                        Entry e;
                        e.id = quint64(o.value("id").toDouble(0));
                        e.blobHash = o.value("hash").toString().toLatin1();
                        e.bytes = qint64(o.value("bytes").toDouble(0));
                        e.lastUsed = qint64(o.value("used").toDouble(0));
                        e.live = o.value("live").toBool(true);
                        e.volRoot = o.value("vol").toString();
                        if (e.id == 0) continue;
                        if (e.id >= nextId) nextId = e.id + 1;
                        entries.insert(path, e);
                        bytes += e.bytes;
                    }
                }
            }
            else {
                QString msg = "Preview cache index unreadable; starting a new cache.";
                G::issue("Warning", msg, "DevelopPreviewCache::load", -1, f.fileName());
            }
        }
        dirty = false;
    }
}

void DevelopPreviewCache::save()
{
    QMutexLocker lk(&mutex);
    ensureLoadedLocked();
    if (!dirty) return;
    const QString d = dir.isEmpty() ? defaultCacheDir() : dir;
    QDir().mkpath(d);

    QJsonArray arr;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        QJsonObject o;
        o.insert("path", it.key());
        o.insert("id", double(it->id));
        o.insert("hash", QString::fromLatin1(it->blobHash));
        o.insert("bytes", double(it->bytes));
        o.insert("used", double(it->lastUsed));
        o.insert("live", it->live);
        o.insert("vol", it->volRoot);
        arr.append(o);
    }
    QJsonObject root;
    root.insert("version", kIndexVersion);
    root.insert("nextId", double(nextId));
    root.insert("entries", arr);

    QSaveFile f(d + "/" + kIndexName);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (f.commit()) dirty = false;
}
