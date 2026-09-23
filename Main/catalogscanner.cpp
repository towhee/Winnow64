#include "Main/catalogscanner.h"
#include "Utilities/catalogloadprobe.h"   // TEMPORARY: catalog load timing
#include "Metadata/keywordpaths.h"
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Utilities/utilities.h"

#include "Cache/mountsnapshot.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
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
    /* scan() is invoked across threads with a queued connection, which cannot marshal a
       type the metatype system has not been told about. */
    qRegisterMetaType<CatalogScope>("CatalogScope");
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
    /*  schema 6/7: the fields a datamodel ROW displays that a search index never
        needed. The scanner was copying NONE of them, so a folder catalogued here and
        the same folder captured by DataModel::catalogRows produced different rows --
        the very thing the comment below forbids.

        orientation is the one that bit. A row served from the index carried 0,
        Thumb::checkOrientation switched on it and matched no case, and every rotated
        image showed its thumbnail on its side -- exactly the failure Cache/catalog.h
        predicts where the field is declared. The unrotated icon was then written to
        the thumbnail index, so it survived until the source file changed.

        pick is NOT here: it is app state the datamodel owns, not something the
        metadata read ever sets, so the scanner must leave it at its default. */
    row.orientation = m.orientation;
    row.exposureComp = m.exposureCompensation;
    row.focusX = m.focusX;
    row.focusY = m.focusY;
    row.email = m.email;
    row.url = m.url;
    row._rating = m._rating;
    row._label = m._label;
    row._creator = m._creator;
    row._title = m._title;
    row._copyright = m._copyright;
    row._email = m._email;
    row._url = m._url;
    row.developed = m.developEdited;
    row.devPreviewKey = m.devPreviewKey;
    row.shootingInfo = m.shootingInfo;
    /* The prefix-expanded PATHS, exactly as DataModel::catalogRows supplies them -- the
       scanner and the opportunistic capture must index the same image the same way, or a
       folder would be catalogued differently depending on which of them saw it first. */
    row.keywords = keywordPrefixExpand(keywordEffectivePaths(m.keywords, m.keywordPaths));
    row.keywordPaths = m.keywordPaths;
    /*  dc:subject AS THE FILE SPELLED IT. The scanner was not filling this, so a row it
        indexed carried an empty keywords_literal while the same row captured from a
        loaded folder carried the real list -- meaning an image was described differently
        depending on which of the two saw it first, which is exactly what the comment
        above forbids. It matters beyond tidiness: the literal list is the only one that
        may ever be written back to a file, and schema 10's rebuild reads it. */
    row.keywordsLiteral = m.keywords;
    return true;
}

void CatalogScanner::scan(const CatalogScope &scope)
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
    /*  TEMPORARY. Selecting the Catalog is what STARTS this (MW::maybeAutoScanCatalog),
        so it runs underneath the load the probe is timing -- walking folders and
        committing to the same catalog the readers are querying per row. */
    CatLoad::note("CatalogScanner::scan STARTED (background)");

    /* Created here, not in the constructor: the constructor runs on the GUI thread
       (before moveToThread), and Metadata must belong to the thread that parses
       with it. */
    if (!metadata) metadata = new Metadata;

    int scanned = 0;
    int indexed = 0;
    int unreadable = 0;
    int newFolders = 0;
    int demoted = 0;
    bool aborted = false;

    /*  THE FOLDERS THE CATALOG ALREADY HELD, read ONCE before anything is written --
        after the first commit of a folder it is no longer new, so a per-folder query
        could not answer this question. One GROUP BY, reused for both halves of the
        reconcile: what is here and was not, and what was here and is not. */
    QSet<QString> known;
    {
        const QMap<QString, int> counts = Catalog::instance().folderCounts();
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
            known.insert(it.key());
    }

    /* Expand the include rows to the folders actually to be walked.
       Utilities::subFolderTree is the same multi-threaded walk the recursive folder load
       uses, so a folder the user could open with Opt-click covers exactly the same
       folders here.

       THE RECURSIVE EXCLUDES ARE HANDED TO THE WALK rather than applied to its result,
       so an excluded hierarchy is never enumerated at all -- which is the point, since a
       big branch is exactly what a user excludes. The non-recursive ones cannot prune a
       descent, so those are filtered out of what comes back. */
    const QStringList prune = catalogScopePrunePaths(scope);
    QStringList folders;
    for (const CatalogScopeEntry &e : scope) {
        if (!e.include || e.path.isEmpty()) continue;
        if (!waitWhilePaused()) { aborted = true; break; }
        /* Normalised here as well as in the editor: the scope can also arrive from
           migrated settings or a self-test, and one trailing slash makes every prefix
           test below quietly false. */
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

    /* Total is FOLDERS, not files -- the file count is not known until each folder is
       enumerated, and a total that kept growing would make the bar run backwards. */
    const int totalFolders = folders.size();
    int folderNo = 0;

    QVector<CatalogRow> batch;
    batch.reserve(kCommitRows);
    /* Files that would not parse, kept apart from the rows that did: they are written by
       a different call, and mixing them would mean inventing metadata for them. */
    QVector<CatalogRow> unreadableBatch;

    for (const QString &folder : folders) {
        if (aborted) break;
        if (!waitWhilePaused()) { aborted = true; break; }
        ++folderNo;

        const QDir dir(folder);
        const QStringList names = dir.entryList(QDir::Files, QDir::NoSort);

        /*  WHAT IS GONE, answered from the listing we already have. dir.exists() is the
            load-bearing guard: an unmounted volume and an empty folder both enumerate to
            nothing, and reconciling against the first would demote a whole drive.

            EVERY FILE GOES IN, ahead of the extension and size filters below. present is
            "what the folder holds", not "what this scan would index" -- a file truncated
            to zero bytes, or one whose format support has since been dropped, is still
            on disk, and leaving it out would demote a row that should stand. */
        if (dir.exists()) {
            QSet<QString> present;
            present.reserve(names.size());
            for (const QString &name : names) present.insert(dir.filePath(name));
            demoted += Catalog::instance().reconcileFolder(folder, present);
        }

        const bool folderIsNew = !known.contains(folder);
        /*  ROWS THIS FOLDER CONTRIBUTED, counted here rather than from the running
            indexed total. indexed only moves when a 200-row batch FLUSHES, and a batch
            spans folders -- so a new folder of five images would leave indexed exactly
            where it found it and never be counted, while whichever folder happened to
            trip the flush would be credited with it. */
        int addedThisFolder = 0;

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
            /* Counted, not just skipped: a file the parser cannot read is a permanent
               gap between the folder and the index, and the editor has to be able to say
               so rather than ask for another scan. */
            if (!parseInto(row)) {
                /* Recorded, not merely counted. A stub row is what makes an unreadable
                   file something the user can list under Availability instead of an
                   unexplained gap between the folder and the index. */
                ++unreadable;
                unreadableBatch.append(stampOnly(row.path));
                if (unreadableBatch.size() >= kCommitRows) {
                    Catalog::instance().commitUnreadable(unreadableBatch);
                    unreadableBatch.clear();
                }
                continue;
            }
            batch.append(row);
            ++addedThisFolder;
            if (batch.size() >= kCommitRows) {
                indexed += Catalog::instance().commit(batch);
                batch.clear();
            }
        }

        /*  A folder the catalog had never seen that yielded nothing -- no supported
            files, or every one of them unreadable -- is not something the user added, so
            it does not count. (The no-supported-files case never reaches here: it
            continues out of the loop above.) */
        if (folderIsNew && addedThisFolder > 0) ++newFolders;

        emit progress(folderNo, totalFolders);
        emit status("Cataloguing " + dir.dirName());
    }

    if (!batch.isEmpty()) indexed += Catalog::instance().commit(batch);
    if (!unreadableBatch.isEmpty())
        Catalog::instance().commitUnreadable(unreadableBatch);

    /*  FOLDERS THAT HAVE GONE FROM DISK ENTIRELY. The loop above reconciles what it
        walks, and a folder that no longer exists is never walked -- so without this its
        rows would stay live forever and its images would keep coming back from a search
        that cannot open one of them.

        ONLY AFTER A COMPLETE PASS. A scan the user stopped has not proved anything about
        the folders it did not reach, and demoting on the strength of a half-finished walk
        is how a library quietly loses half of itself.

        ONLY WHAT THE SCOPE STILL CLAIMS. A folder the scope no longer admits is
        MW::reconcileCatalogToScope's business, and it forgets rather than demotes -- two
        answers to one folder would race.

        ONLY ON A MOUNTED VOLUME, the same guard sweep() takes and for the same reason: an
        ejected card is not a deletion. The snapshot is taken here, after the walk, so its
        window is as short as possible (see Cache/mountsnapshot.h). */
    if (!aborted && !abort.load(std::memory_order_relaxed)) {
        const MountSnapshot mounts = MountSnapshot::take();
        for (const QString &f : std::as_const(known)) {
            if (!waitWhilePaused()) { aborted = true; break; }
            if (!catalogScopeIncludes(scope, f)) continue;
            if (catalogScopeExcludes(scope, f)) continue;
            if (mounts.rootOf(f).isEmpty()) continue;
            if (QDir(f).exists()) continue;
            demoted += Catalog::instance().reconcileFolder(f, QSet<QString>());
        }
    }

    running.store(false, std::memory_order_relaxed);
    CatLoad::note(QString("CatalogScanner::scan FINISHED (scanned %1, indexed %2)")
                      .arg(scanned).arg(indexed));   // TEMPORARY
    emit finished(scanned, indexed, unreadable, newFolders, demoted,
                  aborted || abort.load(std::memory_order_relaxed));
}
