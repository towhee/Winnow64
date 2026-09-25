#ifndef CATALOGENUMERATE_H
#define CATALOGENUMERATE_H

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "Cache/catalog.h"        // CatalogPathInfo
#include "Main/catalogscope.h"

/*
    WHAT THE SCOPE TABLE MEANS ON DISK, stated once. See notes/Documentation.txt
    "Cataloguing Designated Folders" and "Catalog Diagnostics".

    WHY THIS IS ONE PLACE. The Manage Catalog status line compares two numbers -- what
    the folders hold and what the index holds -- and calls their difference work still to
    do. The folder count used to be made by its own walk (includes summed, excludes
    subtracted) while the scanner made a different one, so the two could disagree about
    the same library. "13 images not catalogued yet -- press Scan" was the result: a gap
    no scan could close, because the scan and the count were not walking the same files.
    Every caller now asks these functions, so the scanner, the count and the diagnostics
    report enumerate the same folders and apply the same file rule by construction.

    Qt Core only, so the unit tests can link it without the main window.
*/

/*  The folders a scan walks, in walk order, duplicates removed: each existing include
    root the scope does not exclude, its subtree when the row recurses (recursive
    excludes pruned from the descent, .photoslibrary skipped), and every non-recursive
    exclusion filtered out. keepGoing is asked between roots; returning false stops the
    walk and sets *aborted. */
QStringList catalogScopeFolders(const CatalogScope &scope,
                                const std::function<bool()> &keepGoing = {},
                                bool *aborted = nullptr);

/* The scanner's file rule, by NAME: a supported extension. Size is the caller's business
   (the scanner skips zero-byte files, and a names-only count cannot know). */
bool catalogCandidateName(const QString &name, const QSet<QString> &exts);

/* How many candidate files the scope holds, from names alone -- no stat. The number the
   Manage Catalog status line compares with the index. */
int catalogScopeFileCount(const CatalogScope &scope, const QSet<QString> &exts);

/* "about 2 h 10 m", "about 4 min", "less than a minute"; "estimating…" for secs < 0. */
QString catalogFormatEta(double secs);

/*  ONE FILE ON DISK, as the diagnostics walk found it. size < 0 means "not stat'd". */
struct CatalogDiskFile
{
    QString path;
    qint64 size = -1;
    qint64 mtime = 0;
    qint64 sidecarMtime = 0;
};

/*  Every candidate file the scope holds. With stat, each is stat'd along with its .xmp
    sidecar, exactly as CatalogScanner::stampOnly does, so a comparison with the index's
    stamps means what staleOf means. */
QVector<CatalogDiskFile> catalogScopeFiles(const CatalogScope &scope,
                                           const QSet<QString> &exts, bool stat);

struct CatalogGapItem
{
    QString path;
    QString reason;
};

/*  THE DIFFERENCE BETWEEN THE FOLDERS AND THE INDEX, FILE BY FILE.

    notCatalogued + notOnDisk account for the status line's gap exactly:
        onDisk - rows == notCatalogued.size() - notOnDisk.size() - outsideScope.size()
    so every image the line counts is named here with a reason. */
struct CatalogGapReport
{
    int onDisk = 0;                      // candidate files the scope holds
    int rows = 0;                        // every index row, live or not
    QVector<CatalogGapItem> notCatalogued;
    QVector<CatalogGapItem> notOnDisk;   // in scope, but the walk did not find them
    QVector<CatalogGapItem> outsideScope;
    QVector<CatalogGapItem> stale;       // in both, but the stamps differ
    QVector<CatalogGapItem> unreadable;  // stub rows
    int zeroByte = 0;
    int collisions = 0;
    int neverIndexed = 0;
};

/*  Classify. Pure: the filesystem is reached only through the two callbacks, so the
    tests can drive it with synthetic paths (a case twin cannot exist on a
    case-insensitive test volume).
        keyOf        -- the index's path key (cachePathKey in the app)
        fileExists   -- does a path exist right now
        volumeMounted-- is the volume holding a path mounted */
CatalogGapReport catalogAnalyzeGap(
    const QVector<CatalogDiskFile> &disk,
    const QVector<CatalogPathInfo> &rows,
    const CatalogScope &scope,
    const std::function<QString(const QString &)> &keyOf,
    const std::function<bool(const QString &)> &fileExists,
    const std::function<bool(const QString &)> &volumeMounted);

#endif // CATALOGENUMERATE_H
