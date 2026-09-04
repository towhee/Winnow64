#include "Main/catalogscanner.h"
#include "Metadata/keywordflatten.h"
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Utilities/utilities.h"

#include <QDir>
#include <QFileInfo>
#include <QThread>

namespace {

/* Rows per commit. One transaction per folder would be simpler, but a folder can hold
   tens of thousands of images and the whole batch is lost if the app quits mid-scan;
   committing in chunks bounds what a quit costs to a few hundred files re-stat'd. */
constexpr int kCommitRows = 200;

/* How long to sleep between checks while giving way to a folder load. Short enough to
   resume promptly, long enough that waiting costs nothing. */
constexpr int kPauseSliceMs = 100;

}  // namespace

CatalogScanner::CatalogScanner(QObject *parent)
    : QObject(parent)
{
    moveToThread(&scannerThread);
    scannerThread.start(QThread::LowPriority);
}

CatalogScanner::~CatalogScanner()
{
    /* Ask the scan to end, then let the thread finish the file it is on. The wait is
       bounded because every loop checks abort between files. */
    stop();
    scannerThread.quit();
    scannerThread.wait();
    delete metadata;
}

void CatalogScanner::stop()
{
    abort.store(true, std::memory_order_relaxed);
}

bool CatalogScanner::shouldPause() const
{
/*
    Give way while the datamodel is being modified, or while MW is tearing down in-flight
    work. A folder load is what the user is actually waiting on, and it saturates the
    same disk this scan is reading, so a scan that kept going would make the load
    visibly slower to build an index nobody has asked to see yet.

    G::stop IS A PAUSE HERE, NOT AN ABORT. It is a transient teardown latch that
    MW::stop sets and clears around every folder change (and the memory-overrun path
    sets while it drains). Treating it as an abort would mean the scan died the first
    time the user clicked a folder, so a library scan could never finish on a session
    anybody was using. Pausing still satisfies what those callers want -- no further
    allocation from here while they tear down -- without throwing the pass away.
*/
    return G::isModifyingDatamodel || G::stop;
}

bool CatalogScanner::waitWhilePaused()
{
/*
    Returns false only when the scan has actually been ASKED to end: the user pressed
    Stop, or the object is being destroyed. Everything else is waited out.
*/
    while (shouldPause()) {
        if (abort.load(std::memory_order_relaxed)) return false;
        QThread::msleep(kPauseSliceMs);
    }
    return !abort.load(std::memory_order_relaxed);
}

CatalogRow CatalogScanner::stampOnly(const QString &fPath)
{
/*
    Everything staleOf needs, from two stats and no parsing. The sidecar stamp is the
    load-bearing one for a raw library: Lightroom rewrites the .xmp and never touches the
    NEF, so an image whose keywords changed looks untouched without it.
*/
    const QFileInfo fi(fPath);
    CatalogRow r;
    r.path = fPath;
    r.folder = fi.absoluteDir().path();
    r.filename = fi.fileName();
    r.ext = fi.suffix().toLower();
    r.srcSize = fi.size();
    r.srcMtime = fi.lastModified().toSecsSinceEpoch();

    const QFileInfo si(fi.absoluteDir().path() + "/" + fi.completeBaseName() + ".xmp");
    if (si.exists()) r.sidecarMtime = si.lastModified().toSecsSinceEpoch();
    return r;
}

bool CatalogScanner::parseInto(CatalogRow &row)
{
/*
    Read the image's metadata and copy the catalogued fields across. isLoadXmp is true
    because the keywords are the point of the exercise -- without it dc:subject and
    lr:hierarchicalSubject are never read and every row would be catalogued blank.
*/
    const QFileInfo fi(row.path);
    if (!metadata->loadImageMetadata(fi, 0, G::dmInstance, true, true, false, true,
                                     "CatalogScanner::parseInto")) {
        return false;
    }
    const ImageMetadata &m = metadata->m;

    row.captured = m.createdDate;
    row.rating = m.rating.toInt();
    row.label = m.label;
    row.title = m.title;
    row.creator = m.creator;
    row.copyright = m.copyright;
    row.make = m.make;
    row.model = m.model;
    row.lens = m.lens;
    row.iso = m.ISONum;
    row.aperture = m.apertureNum;
    row.shutter = m.exposureTimeNum;
    row.focalLength = m.focalLengthNum;
    row.width = m.width;
    row.height = m.height;
    row.gpsCoord = m.gpsCoord;
    /* The FLAT vocabulary, exactly as DataModel::catalogRows supplies it -- the scanner
       and the opportunistic capture must index the same image the same way, or a folder
       would be catalogued differently depending on which of them saw it first. */
    row.keywords = flattenKeywords(m.keywords, m.keywordPaths);
    row.keywordPaths = m.keywordPaths;
    return true;
}

bool CatalogScanner::isExcluded(const QString &folder, const QStringList &excludes)
{
/*
    An exclusion covers the folder itself AND everything below it -- that is the whole
    point of it: the user includes a library and carves one branch out of it.

    THE TRAILING SLASH IS LOAD-BEARING. A plain startsWith would make "/Photos/2024"
    exclude "/Photos/2024 raw" as well, which is a silent and very hard to spot way to
    lose half a library from the index.
*/
    for (const QString &ex : excludes) {
        if (ex.isEmpty()) continue;
        QString e = ex;
        while (e.endsWith('/')) e.chop(1);
        if (folder == e) return true;
        if (folder.startsWith(e + "/")) return true;
    }
    return false;
}

void CatalogScanner::scan(const QStringList &roots, bool recurse,
                          const QStringList &excludes)
{
/*
    Walk every root, catalogue what has changed, and report as it goes.

    THE WHOLE PASS IS BOUNDED BY THE ABORT CHECKS, not by its own size: a library scan is
    minutes to hours, and the user must be able to change folders, quit, or turn the scan
    off at any point in it without waiting.
*/
    if (G::isLogger) G::log("CatalogScanner::scan");

    if (running.exchange(true, std::memory_order_relaxed)) return;   // already scanning
    abort.store(false, std::memory_order_relaxed);

    /* Created here, not in the constructor: the constructor runs on the GUI thread
       (before moveToThread), and Metadata must belong to the thread that parses
       with it. */
    if (!metadata) metadata = new Metadata;

    int scanned = 0;
    int indexed = 0;
    bool aborted = false;

    /* Expand the roots to the folders actually to be walked. Utilities::subFolderTree is
       the same multi-threaded walk the recursive folder load uses, so a root the user
       could open with Opt-click covers exactly the same folders here. */
    QStringList folders;
    for (const QString &root : roots) {
        if (!waitWhilePaused()) { aborted = true; break; }
        if (!QFileInfo::exists(root)) continue;      // unmounted volume, or moved
        if (isExcluded(root, excludes)) continue;    // a root inside an excluded tree
        folders << root;
        if (recurse) {
            /* The exclusions are handed to the walk rather than applied to its result,
               so an excluded hierarchy is never enumerated at all. */
            QStringList subDirs;
            Utilities::subFolderTree(root, subDirs, excludes);
            for (const QString &d : subDirs) {
                /* Same exclusion as the folder load: a .photoslibrary holds thousands of
                   derivative masters per photo and would swamp the catalog. */
                if (d.contains(".photoslibrary")) continue;
                folders << d;
            }
        }
    }
    folders.removeDuplicates();

    /* Total is FOLDERS, not files -- the file count is not known until each folder is
       enumerated, and a total that kept growing would make the bar run backwards. */
    const int totalFolders = folders.size();
    int folderNo = 0;

    QVector<CatalogRow> batch;
    batch.reserve(kCommitRows);

    for (const QString &folder : folders) {
        if (aborted) break;
        if (!waitWhilePaused()) { aborted = true; break; }
        ++folderNo;

        const QDir dir(folder);
        const QStringList names = dir.entryList(QDir::Files, QDir::NoSort);

        /* Stat everything first, then ask the catalog which of them actually need
           reading. On a rescan this is the entire cost of the folder. */
        QList<CatalogRow> candidates;
        candidates.reserve(names.size());
        for (const QString &name : names) {
            const int dot = name.lastIndexOf('.');
            if (dot < 0) continue;
            const QString ext = name.mid(dot + 1).toLower();
            if (!metadata->supportedFormats.contains(ext)) continue;
            const QString fPath = dir.filePath(name);
            const QFileInfo fi(fPath);
            if (fi.size() == 0) continue;
            candidates.append(stampOnly(fPath));
        }
        if (candidates.isEmpty()) {
            emit progress(folderNo, totalFolders);
            continue;
        }

        const QSet<QString> stale = Catalog::instance().staleOf(candidates);
        scanned += candidates.size();

        for (CatalogRow &row : candidates) {
            if (!stale.contains(row.path)) continue;
            if (!waitWhilePaused()) { aborted = true; break; }
            if (!parseInto(row)) continue;
            batch.append(row);
            if (batch.size() >= kCommitRows) {
                indexed += Catalog::instance().commit(batch);
                batch.clear();
            }
        }

        emit progress(folderNo, totalFolders);
        emit status("Cataloguing " + dir.dirName());
    }

    if (!batch.isEmpty()) indexed += Catalog::instance().commit(batch);

    running.store(false, std::memory_order_relaxed);
    emit finished(scanned, indexed, aborted || abort.load(std::memory_order_relaxed));
}
