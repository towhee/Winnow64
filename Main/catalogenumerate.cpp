#include "Main/catalogenumerate.h"
#include "Utilities/utilities.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <cmath>

QStringList catalogScopeFolders(const CatalogScope &scope,
                                const std::function<bool()> &keepGoing,
                                bool *aborted)
{
/*
    Moved here from CatalogScanner::scan so the count and the diagnostics walk exactly
    what the scanner walks.

    THE RECURSIVE EXCLUDES ARE HANDED TO THE WALK rather than applied to its result, so
    an excluded hierarchy is never enumerated at all -- which is the point, since a big
    branch is exactly what a user excludes. The non-recursive ones cannot prune a
    descent, so those are filtered out of what comes back.
*/
    if (aborted) *aborted = false;
    const QStringList prune = catalogScopePrunePaths(scope);
    QStringList folders;
    for (const CatalogScopeEntry &e : scope) {
        if (!e.include || e.path.isEmpty()) continue;
        if (keepGoing && !keepGoing()) {
            if (aborted) *aborted = true;
            break;
        }
        /* Normalised here as well as in the editor: the scope can also arrive from
           migrated settings or a self-test, and one trailing slash makes every prefix
           test quietly false. */
        const QString root = catalogScopeNormalize(e.path);
        if (!QFileInfo::exists(root)) continue;      // unmounted volume, or moved
        if (!catalogScopeExcludes(scope, root)) folders << root;
        if (e.recurse) {
            QStringList subDirs;
            Utilities::subFolderTree(root, subDirs, prune);
            for (const QString &d : subDirs) {
                /* Same exclusion as the folder load: a .photoslibrary holds thousands of
                   derivative masters per photo and would swamp the catalog. */
                if (d.contains(".photoslibrary")) continue;
                if (catalogScopeExcludes(scope, d)) continue;
                folders << d;
            }
        }
    }
    folders.removeDuplicates();
    return folders;
}

bool catalogCandidateName(const QString &name, const QSet<QString> &exts)
{
    const int dot = name.lastIndexOf('.');
    if (dot < 0) return false;
    return exts.contains(name.mid(dot + 1).toLower());
}

int catalogScopeFileCount(const CatalogScope &scope, const QSet<QString> &exts)
{
    int n = 0;
    const QStringList folders = catalogScopeFolders(scope);
    for (const QString &f : folders) {
        const QStringList names = QDir(f).entryList(QDir::Files, QDir::NoSort);
        for (const QString &name : names)
            if (catalogCandidateName(name, exts)) ++n;
    }
    return n;
}

QString catalogFormatEta(double secs)
{
    if (secs < 0) return QString::fromUtf8("estimating…");
    if (secs < 60) return "less than a minute";
    const qint64 mins = qint64(std::ceil(secs / 60.0));
    if (mins < 60) return QString("about %1 min").arg(mins);
    const qint64 h = mins / 60;
    const qint64 m = mins % 60;
    if (m == 0) return QString("about %1 h").arg(h);
    return QString("about %1 h %2 m").arg(h).arg(m);
}

QVector<CatalogDiskFile> catalogScopeFiles(const CatalogScope &scope,
                                           const QSet<QString> &exts, bool stat)
{
    QVector<CatalogDiskFile> out;
    const QStringList folders = catalogScopeFolders(scope);
    for (const QString &f : folders) {
        const QDir dir(f);
        const QStringList names = dir.entryList(QDir::Files, QDir::NoSort);
        for (const QString &name : names) {
            if (!catalogCandidateName(name, exts)) continue;
            CatalogDiskFile d;
            d.path = dir.filePath(name);
            if (stat) {
                /* The same two stats CatalogScanner::stampOnly makes, so the comparison
                   with the index's stamps is the one staleOf makes. */
                const QFileInfo fi(d.path);
                d.size = fi.size();
                d.mtime = fi.lastModified().toSecsSinceEpoch();
                const QFileInfo si(dir.path() + "/" + fi.completeBaseName() + ".xmp");
                if (si.exists()) d.sidecarMtime = si.lastModified().toSecsSinceEpoch();
            }
            out.append(d);
        }
    }
    return out;
}

CatalogGapReport catalogAnalyzeGap(
    const QVector<CatalogDiskFile> &disk,
    const QVector<CatalogPathInfo> &rows,
    const CatalogScope &scope,
    const std::function<QString(const QString &)> &keyOf,
    const std::function<bool(const QString &)> &fileExists,
    const std::function<bool(const QString &)> &volumeMounted)
{
/*
    Match every disk file to the index row with its path key. What is left over on
    either side IS the gap, and each leftover gets the reason it is left over.

    A KEY IS CONSUMED BY THE FIRST FILE THAT MATCHES IT. Two files whose names differ
    only in case (or in Unicode normalisation) share one key and so one row -- the second
    can never be catalogued separately, and saying so is the only useful thing to say.
    A file whose own spelling is the row's is preferred, so the twin reported is the one
    the index really does not hold.
*/
    CatalogGapReport r;
    r.onDisk = disk.size();
    r.rows = rows.size();

    QHash<QString, int> rowOfKey;
    rowOfKey.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const CatalogPathInfo &p = rows.at(i);
        rowOfKey.insert(p.pathKey.isEmpty() ? keyOf(p.path) : p.pathKey, i);
        if (p.unreadable)
            r.unreadable.append({p.path, "could not be read (unsupported or damaged)"});
    }

    QVector<QString> keys(disk.size());
    for (int i = 0; i < disk.size(); ++i) keys[i] = keyOf(disk.at(i).path);

    /* Which disk file owns each key: the exact spelling first, then the first seen. */
    QHash<QString, int> ownerOfKey;
    ownerOfKey.reserve(disk.size());
    for (int i = 0; i < disk.size(); ++i) {
        const auto it = rowOfKey.constFind(keys.at(i));
        if (it != rowOfKey.constEnd() && rows.at(it.value()).path == disk.at(i).path)
            ownerOfKey.insert(keys.at(i), i);
    }
    for (int i = 0; i < disk.size(); ++i)
        if (!ownerOfKey.contains(keys.at(i))) ownerOfKey.insert(keys.at(i), i);

    QVector<bool> rowMatched(rows.size(), false);
    for (int i = 0; i < disk.size(); ++i) {
        const CatalogDiskFile &d = disk.at(i);
        const QString &key = keys.at(i);
        const int owner = ownerOfKey.value(key, i);
        if (owner != i) {
            ++r.collisions;
            r.notCatalogued.append({d.path,
                QString("same index key as %1 (names differ only in case or Unicode "
                        "form) -- only one of the two can be catalogued")
                    .arg(disk.at(owner).path)});
            continue;
        }
        const auto it = rowOfKey.constFind(key);
        if (it == rowOfKey.constEnd()) {
            if (d.size == 0) {
                ++r.zeroByte;
                r.notCatalogued.append({d.path,
                                        "zero-byte file (skipped by the scanner)"});
            }
            else {
                ++r.neverIndexed;
                r.notCatalogued.append({d.path,
                    "not in the index -- a scan has not reached it yet, or it failed "
                    "to commit"});
            }
            continue;
        }
        const CatalogPathInfo &row = rows.at(it.value());
        rowMatched[it.value()] = true;
        if (row.unreadable) continue;              // listed with the stubs
        if (d.size >= 0 && (row.srcSize != d.size || row.srcMtime != d.mtime
                            || row.sidecarMtime != d.sidecarMtime)) {
            r.stale.append({d.path, row.sidecarMtime != d.sidecarMtime
                                        ? "sidecar changed since it was indexed"
                                        : "file changed since it was indexed"});
        }
        else if (!row.live) {
            r.stale.append({d.path, "on disk, but its row is demoted (next scan "
                                    "restores it)"});
        }
    }

    for (int i = 0; i < rows.size(); ++i) {
        if (rowMatched.at(i)) continue;
        const CatalogPathInfo &p = rows.at(i);
        const QString folder = p.folder.isEmpty() ? QFileInfo(p.path).absolutePath()
                                                  : p.folder;
        if (!catalogScopeIncludes(scope, folder) || catalogScopeExcludes(scope, folder)) {
            r.outsideScope.append({p.path, "folder is not in the scope table"});
            continue;
        }
        if (!volumeMounted(p.path)) {
            r.notOnDisk.append({p.path, "volume not mounted"});
        }
        else if (fileExists(p.path)) {
            r.notOnDisk.append({p.path,
                "on disk, but the scope walk does not reach it (hidden folder, "
                ".photoslibrary, or an unsupported extension)"});
        }
        else {
            r.notOnDisk.append({p.path, p.live
                ? "file is gone -- row still live (the next scan demotes it)"
                : "file is gone -- row demoted"});
        }
    }
    return r;
}
