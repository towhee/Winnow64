#ifndef DUPLICATES_H
#define DUPLICATES_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <atomic>
#include "Cache/duplicateclassify.h"

class Metadata;

/*
    LIBRARY DUPLICATES. A duplicate is another Library image with the same capture time,
    file type and pixels -- or, for a video, the same capture time, file type and
    duration. See notes/Documentation.md "Library Duplicates".

    THREE STAGES, each as cheap as it can be:

    1. CANDIDATES come straight from the index: rows sharing (captured, ext, width,
       height) -- Catalog::duplicateCandidates, an image_dupkey GROUP BY. Almost every
       image in a library shares that with nothing and is never opened.
    2. FINGERPRINTS are taken for candidates only, once per file content, and stored in
       the row (image.pixelhash / image.duration_ms, schema 32). An image is decoded as
       Winnow decodes it in Preview mode -- the embedded full-size JPEG of a raw, the
       file itself otherwise -- WITHOUT applying orientation, so a copy rotated in Winnow
       still matches, and its pixels are SHA-1'd. A video's mvhd duration is read
       (MOV::durationMs, two seeks); a video format with no parser has no duration and is
       not checked. Burst frames share a capture second and differ here.
    3. STATUS is derived from the fingerprinted rows and the user's okayed set
       (Datamodel/duplicateok.h) by classifyDuplicates -- in memory, no decode, so
       okaying an image or trashing one costs a query of the few fingerprinted rows.

    THREADS. DuplicateChecker lives on its own low-priority thread and decodes on a small
    private pool; it gives way to folder loads exactly as CatalogScanner does. Duplicates
    (GUI thread) owns it and holds the current answer, which the datamodel column, the
    thumbnail badge, the Filters category and Manage Catalog read.

    KEPT CURRENT by the catalog's hooks (Catalog::setContentChangedHook /
    setMembershipChangedHook): rows added or whose content changed are checked (their
    key-mates fingerprinted as needed), and rows deleted, demoted, forgotten or moved
    re-derive the status. Both are debounced here.
*/

struct DuplicateProgress
{
    int done = 0;
    int total = 0;
    bool full = false;          // the Manage Catalog check, not an incremental one
    bool paused = false;
    double etaSecs = -1;
};
Q_DECLARE_METATYPE(DuplicateProgress)

class DuplicateChecker : public QObject
{
    Q_OBJECT
public:
    explicit DuplicateChecker(QObject *parent = nullptr);
    ~DuplicateChecker() override;
    void shutdown(int maxWaitMs = 3000);
    /* Stop whatever is running (shutdown). Safe from any thread. */
    void stop();
    /*  Stop the full check numbered gen -- running, or still queued behind an incremental
        one, which this must not stop instead. Safe from any thread. */
    void stopFull(int gen) { cancelFullGen.store(gen, std::memory_order_relaxed); }
    bool isRunning() const { return running.load(std::memory_order_relaxed); }

public slots:
    /* Fingerprint every candidate that has none, then derive the status. */
    void checkAll(int gen, const QSet<QString> &okPaths);
    /* The same for these rows and the rows they share a candidate key with. */
    void checkKeys(const QStringList &pathkeys, const QSet<QString> &okPaths);
    /* Derive the status from what is stored, decoding nothing. */
    void rebuild(const QSet<QString> &okPaths);

signals:
    /* Throttled to four a second. An incremental check reports only once it has run for
       a second, so checking the two files an ingest made never flashes a progress row. */
    void progress(const DuplicateProgress &p);
    void finished(bool full, bool cancelled, int fingerprinted);
    void rebuilt(const QHash<QString, DuplicateInfo> &status, const DuplicateCounts &counts);

private:
    QThread checkerThread;
    std::atomic<bool> abort{false};
    std::atomic<bool> closing{false};
    std::atomic<bool> running{false};
    std::atomic<int> cancelFullGen{0};
    int fullGen = 0;                // the full check running now, checker thread
    /* Read on the checker thread to tell a video from an image. */
    QSet<QString> videoExts;

    int fingerprint(QVector<DuplicateRow> rows, bool full);
    bool shouldPause() const;
    bool waitWhilePaused();
};

class Duplicates : public QObject
{
    Q_OBJECT
public:
    static Duplicates &instance();

    /* Create the checker and register the catalog hooks. MW calls it once at startup;
       nothing is checked or derived before. */
    void start();
    void shutdown();

    /* The Manage Catalog "Duplicates" button: check the whole Library. */
    void checkAll();
    void stop();
    bool isCheckingAll() const { return checkingAll; }
    /* Re-derive the status now (debounced) -- an okay was toggled, rows went. */
    void requestRebuild();

    /* The current answer, GUI thread. */
    const QHash<QString, DuplicateInfo> &status() const { return statusByPath; }
    DuplicateInfo infoOf(const QString &path) const { return statusByPath.value(path); }
    /* The other images in path's group, sorted; empty when it is in none. */
    QStringList twinsOf(const QString &path) const;
    DuplicateCounts counts() const { return lastCounts; }
    /* Whether a whole-Library check has ever completed on this index. */
    bool hasCheckedAll();

signals:
    void statusChanged();
    void progress(const DuplicateProgress &p);
    void finished(bool full, bool cancelled);

private:
    Duplicates() = default;
    void onContentChanged(const QStringList &pathkeys);
    void flushPendingKeys();

    DuplicateChecker *checker = nullptr;
    QHash<QString, DuplicateInfo> statusByPath;
    QHash<int, QStringList> pathsByGroup;     // DuplicateInfo::group -> its paths, sorted
    DuplicateCounts lastCounts;
    bool checkingAll = false;
    int fullGen = 0;
    QStringList pendingKeys;
    bool keysQueued = false;
    bool rebuildQueued = false;
    int checkedAllCache = -1;       // -1 = not read yet
};

#endif // DUPLICATES_H
