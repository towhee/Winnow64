#ifndef MOUNTSNAPSHOT_H
#define MOUNTSNAPSHOT_H

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QStorageInfo>

/*
    Two small value types shared by every tenant of the local index database
    (Cache/cachedb.h): "which volumes are mounted right now" and "what does the file at
    this path look like right now".

    They started inside devpreviewcache.cpp and moved here when the catalog became the
    second thing that has to answer the same two questions. Both tenants index paths
    across removable media, and both have exactly one dangerous verdict to avoid --
    treating an EJECTED volume as a mass deletion -- so they must decide it the same way
    or the two indexes will disagree about the same file.
*/

/*
    A snapshot of the currently mounted volume roots.

    Taking one walks the whole mount table, which costs a statfs per volume. A sweep that
    took a fresh one FOR EVERY ROW would, at the 250,000 rows these indexes are sized for,
    walk the mount table a quarter of a million times. One snapshot per pass turns the
    per-row cost into a list lookup.

    Deliberately a value, not a cached singleton with a time-to-live. The verdict it feeds
    -- "the file is missing, so demote the row" -- is only safe while the volume really is
    mounted, and a stale snapshot that still listed an ejected card would demote every row
    on it. A caller takes a snapshot when it starts and lives with the window it already
    had.
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

    /* A row written before volRoot was recorded, or one on the boot volume, is treated as
       mounted -- the boot volume is always there. */
    bool isMounted(const QString &volRoot) const
    {
        if (volRoot.isEmpty()) return true;
        return roots.contains(volRoot);
    }

    QStringList roots;
};

/*
    What the file at path looks like right now: its length and last-modified time. Used to
    confirm that the image a row was made from is still the image sitting at that path.
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

    /* Does what is on disk CONTRADICT the row -- i.e. is this a different image now?

       Nothing on disk at all is deliberately NOT a contradiction. A missing source is the
       sweep's demote case: the file is in the trash or its volume is ejected, and the row
       is still the right description of it. Only a file that is present and DIFFERENT
       means the path has been reused, which is the case that would otherwise attribute
       one image's data to another.

       A row with nothing recorded cannot contradict anything until a sweep stamps it.
    */
    bool contradicts(qint64 entrySize, qint64 entryMtime) const
    {
        if (!entrySize && !entryMtime) return false;
        if (!valid) return false;
        return size != entrySize || mtime != entryMtime;
    }
};

#endif // MOUNTSNAPSHOT_H
