#ifndef CATALOGSCANNER_H
#define CATALOGSCANNER_H

#include <QDateTime>
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <atomic>

#include "Cache/catalog.h"
#include "Main/catalogscope.h"

class Metadata;

/*  WHERE A RUNNING SCAN IS, sent to the GUI at most four times a second.

    TWO PHASES, BECAUSE ONLY THE SECOND HAS A HONEST TOTAL. Checking walks every folder,
    stats every file and asks the catalog which are stale -- cheap per file, and it is
    what finds out how much real work there is. Indexing then parses exactly those files,
    so done/total is a count of parses and the rate is a parse rate. A single pass with a
    folder total (what this replaced) moved once per folder -- a folder of 10,000 images
    sat still for an hour -- and could not estimate anything, because files skipped as
    unchanged cost microseconds and files parsed cost tens of milliseconds.

    filesPerSec and etaSecs are over ACTIVE time (in Checking the rate is folders per
    second and the estimate covers the checking only): time spent paused
    for a folder load is left out, or browsing during a scan would make the estimate
    climb for no reason. etaSecs < 0 means there is not enough data to say yet. */
struct CatalogScanProgress
{
    enum Phase { Checking, Indexing };
    int phase = Checking;
    int done = 0;
    int total = 0;
    double filesPerSec = 0;
    double etaSecs = -1;
    bool paused = false;
    QString folder;           // the folder being worked on, for the status line
};
Q_DECLARE_METATYPE(CatalogScanProgress)

/*  WHERE THE TIME WENT in the last scan, for the Catalog Diagnostics report. Recorded
    because the decision about parallel parsing waits on it: a 100k-image scan takes
    hours, and whether that is parsing, stat'ing a network volume or committing is the
    question these numbers answer. All milliseconds are active time. */
struct CatalogScanStats
{
    QDateTime started;
    QDateTime ended;
    bool ran = false;
    bool aborted = false;
    int folders = 0;
    int scanned = 0;          // candidate files stat'd
    int stale = 0;            // of those, needing a parse
    int parsed = 0;           // parse attempts made
    int indexed = 0;
    int unreadable = 0;
    int zeroByte = 0;
    int collisions = 0;       // same folder, same path key (case / Unicode twins)
    int newFolders = 0;
    int demoted = 0;
    qint64 walkMs = 0;        // expanding the scope to folders
    qint64 listMs = 0;        // directory listings
    qint64 reconcileMs = 0;
    qint64 stampMs = 0;       // stat of each file and its sidecar
    qint64 staleMs = 0;       // the staleOf queries
    qint64 parseMs = 0;
    qint64 commitMs = 0;
    qint64 pausedMs = 0;      // given way to folder loads
};

/*
    Walks the folders the user's scope table includes (minus the branches it excludes)
    and indexes what it finds, so search covers a library BEFORE the user has browsed it. See
    notes/Documentation.txt "Cataloguing Designated Folders".

    WHY IT IS SEPARATE FROM THE OPPORTUNISTIC CAPTURE. Opening a folder catalogues it
    (MW::folderChangeCompleted), which is free -- the metadata has just been read anyway.
    That only ever covers folders the user has actually visited, so a search on a fresh
    install finds nothing and there is no way to say "index my library". This is that way.

    WHAT IT MAY WALK IS Main/catalogscope.h -- one ordered table of include/exclude rows,
    each with its own subfolder reach. An exclude always wins over an include.

    IT READS METADATA AND NOTHING ELSE. No icon, no decode, no preview: a catalog row is
    text, and rendering anything would turn a background index into hours of GPU work.

    IT OWNS ITS OWN Metadata INSTANCE, created on the scanner thread -- the same rule
    Cache/reader.cpp follows, because Metadata holds one reusable ImageMetadata and one
    parser set and is not safe to share across threads.

    IT YIELDS TO THE USER, and that is the whole reason it is one thread rather than a
    pool. Browsing must not get slower because an index is being built: the scan pauses
    whenever the datamodel is being modified (a folder load is in progress) and abandons
    itself on G::stop. The same courtesy the devPreview builder shows by declining to run
    inside Develop.

    IT SKIPS WORK ALREADY DONE. Every folder is stat'd first and handed to
    Catalog::staleOf, so a rescan of an unchanged root costs one stat per file and no
    parsing at all. That is what makes "Scan now" cheap enough to offer as a button.

    IT RECONCILES WHAT IT WALKS, both ways. Indexing alone only ever grows the catalog,
    so a scan of a library the user had been deleting from left every removed image
    findable. Each folder's listing is complete by the time it has been read, which is
    precisely Catalog::reconcileFolder's precondition -- so the walk answers "what is
    gone" for free, with no second pass and no stat. Folders that have gone from disk
    entirely are handled after the walk, because a folder that no longer exists is never
    walked and so would otherwise never be reconciled at all.

    NOTHING HERE IS AUTHORITATIVE. Like the rest of the catalog it only builds an index;
    it never writes to an image or a sidecar.
*/
class CatalogScanner : public QObject
{
    Q_OBJECT

public:
    explicit CatalogScanner(QObject *parent = nullptr);
    ~CatalogScanner() override;

    bool isRunning() const { return running.load(std::memory_order_relaxed); }
    /*  Stop, AND WAIT for the scanner thread to finish the file it is on (bounded). Call
        before the process can exit: stop() alone only raises a flag, and a quit from the
        Dock, logout or shutdown reaches exit() without destroying MW -- so the destructor
        never runs and the thread was still parsing while exit() destroyed the globals it
        uses (crash reports 2026-09-24: G::issueDedup's hash, QtSql's registry).
        Idempotent; the destructor calls it too. GUI thread. */
    void shutdown(int maxWaitMs = 5000);

    /* The last scan's (or the running scan's) figures. Any thread. */
    CatalogScanStats lastStats() const;

    /* This object lives here, so scan() never runs on the GUI thread. Owned rather than
       managed by MW, following Cache/metaread.h. */
    QThread scannerThread;

public slots:
    /* Scan what the scope table says to scan. Runs on whatever thread this object lives
       on, which MW makes a dedicated one -- never call it directly from the GUI
       thread. */
    void scan(const CatalogScope &scope);
    /* Ask the running scan to stop. Safe from any thread; the scan notices between
       files, so it ends promptly but not instantly. */
    void stop();

signals:
    /* Throttled to four a second: at a hundred thousand images a signal per file would
       cost more than the indexing. See CatalogScanProgress. */
    void progress(const CatalogScanProgress &p);
    /*  indexed = rows actually written (unchanged files are skipped, so this is usually
        far smaller than the number scanned). unreadable = files the scan WANTED to index
        and could not parse.

        UNREADABLE IS REPORTED BECAUSE IT IS PERMANENT. Those files are counted on disk
        and absent from the index for good, so without this number the editor can only
        say "N images not catalogued yet -- press Scan" about a gap that pressing Scan
        will never close. A count nobody can act on has to be labelled as such. */
    /*  newFolders = folders walked that the catalog had never held a row for and that
        this pass indexed at least one image from. It is the count a user recognises --
        "you added three folders" -- where indexed is a number of files they never
        counted themselves.

        demoted = live rows whose file the walk did NOT find, set live = 0. DEMOTED, NOT
        DELETED, exactly as sweep() does: the row comes back on its own the next time a
        commit sees the file, so a folder that was briefly unreachable costs a rescan
        rather than its catalogued keywords. */
    void finished(int scanned, int indexed, int unreadable,
                  int newFolders, int demoted, bool aborted);

private:
    /* True when the scan should give way -- a folder load is running, or the app is
       shutting down. */
    bool shouldPause() const;
    /* Block while shouldPause(), returning false if we were asked to stop instead. Time
       spent blocked is added to pausedMs, and a paused progress report is sent so the
       UI can say why nothing is moving. */
    bool waitWhilePaused();
    /* Send p if a quarter second has passed since the last one, or if force. */
    void report(const CatalogScanProgress &p, bool force = false);

    /* Fill a CatalogRow from what is on disk WITHOUT parsing the image: path, folder,
       size, mtimes. That is everything staleOf needs to decide whether parsing is
       worth doing. */
    static CatalogRow stampOnly(const QString &fPath);
    /* Parse fPath and fill the rest of row. Returns false if the file could not be
       read, in which case it is not catalogued. */
    bool parseInto(CatalogRow &row);

    Metadata *metadata = nullptr;      // created lazily, on the scanner thread
    mutable QMutex statsMutex;
    CatalogScanStats stats;            // guarded by statsMutex
    CatalogScanProgress lastProgress;  // scanner thread only
    QElapsedTimer clock;               // scanner thread only; runs for a scan
    qint64 lastReportMs = -1;          // scanner thread only
    qint64 pausedMs = 0;               // scanner thread only
    std::atomic<bool> abort{false};
    std::atomic<bool> running{false};
};

#endif // CATALOGSCANNER_H
