#ifndef SCROLLVERIFY_H
#define SCROLLVERIFY_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>

class DataModel;
class Metadata;

/*
    IS THE ROW ON SCREEN STILL TRUE? -- the scroll-in stat verification.

    WHAT IT IS FOR. A row filled from the local index (Catalog scope, or a folder load
    with G::useIndexMetadata on) is never opened, so nothing about it is checked against
    the file it names. That is the whole point -- a browse of 43,000 images cannot open
    43,000 files -- but it leaves one gap: edit an image's keywords in Lightroom, which
    rewrites the .xmp and never touches the raw, and Winnow goes on showing what the
    catalog remembers. Nothing revisits the row, so nothing corrects it.

    WHY VERIFYING ON SCROLL IS THE RIGHT SHAPE. Freshness needs the file's size and
    mtime and its sidecar's mtime, which means a stat -- and a stat is ~137x cheaper
    than a read (11.8 us against 1,616 us per row, measured by --catalogprobe on the
    real library). Stat-ing a 10,000-row icon window costs 118 ms where re-reading it
    costs 16 s. So the check is affordable for the rows the user is LOOKING AT and
    nowhere near affordable for the whole set, which is exactly the split a scroll
    position already draws.

    IT ASKS ABOUT THE VISIBLE WINDOW, NOT THE ICON CHUNK. The chunk is a memory budget
    and can be tens of thousands of rows; the window is what a person can see. A row
    scrolled past unseen is not worth a syscall.

    COALESCED, because updateIconRange fires on every scroll event and every selection
    change. The timer restarts on each one and the pass runs when the view settles, so
    dragging a scrollbar the length of a catalog costs one pass, not a thousand.

    ONCE PER ROW, THEN AGAIN AFTER kReverifySecs. Verifying a row on every settle would
    re-stat the same window each time the user rocks the scrollbar; verifying it only
    once per scope would mean a file edited in Lightroom AFTER the user had scrolled
    past it stayed stale for the rest of the session -- which is the bug this exists to
    fix, just later. So the answer is remembered with a timestamp and expires.

    KEYED BY PATH, NOT BY ROW, for the reason the icon store is: rows shift under sorting
    and filtering, and an answer held by row would be attached to a different image the
    moment the user re-sorts.

    AND THE WHOLE SET, ONCE, AFTER A LOAD. The paragraph above is about the rows a person
    is looking at, and it leaves the other 40,000 unverified until they scroll to them.
    verifyWholeSet closes that: the same stat, the same question and the same repair, run
    over every loaded row in the background once the load has settled. It is affordable
    for the reason the scroll pass is -- a stat is ~137x cheaper than a read, and the
    measured rate on the real library is 41,464 rows in about 3.6 s -- but it is NOT
    affordable in one gulp on the GUI thread, so it is paged: a page of rows is collected
    here, stat'd on a pool thread, and the next page is scheduled behind it.

    THE SCROLL PASS OUTRANKS IT. Both use the one in-flight slot, and a page defers while
    a scroll pass holds it. The rows on screen are the ones whose staleness the user can
    actually see; the sweep can wait 50 ms.

    WHAT IT DOES NOT DO. It does not re-read anything and it does not touch the model:
    the pass runs on a pool thread and reports paths. MW::refreshStaleRows owns what
    happens next -- clearing the row's metadata and icon so MetaRead reads it again, and
    committing the re-read row back to the catalog so the index stops being wrong. That
    commit is the half MW::folderChangeCompleted deliberately leaves to this pass: a
    hydrated catalog scope has nothing to commit EXCEPT the rows that turned out stale.
*/
class ScrollVerify : public QObject
{
    Q_OBJECT

public:
    ScrollVerify(DataModel *dm, Metadata *metadata, QObject *parent = nullptr);

    /*  How long a verified answer is trusted before the row is stat'd again, and how
        long the view must be still before a pass runs. */
    static constexpr int kReverifySecs = 60;
    static constexpr int kSettleMs = 400;

public slots:
    /*  The visible range may have changed. Cheap and idempotent: it restarts the settle
        timer and returns. GUI thread. */
    void viewChanged();

    /*  A new set is loading. Forget every answer -- they were about other images, and
        abandon any whole-set sweep still walking the old one. */
    void reset();

    /*  VERIFY EVERY LOADED ROW, ONCE, IN THE BACKGROUND. Called when a load has settled.
        Starts after kWholeSetStartMs so it does not stat 41,000 files while MetaRead is
        still opening the ones it could not serve from the index. Safe to call again while
        a sweep is running -- it restarts from the top, which is what a new load wants. */
    void verifyWholeSet();

signals:
    /*  These paths are indexed, were served from the index, and no longer match their
        files. Delivered on the GUI thread. */
    void rowsAreStale(const QStringList &paths);

private:
    void runPass();
    void runWholeSetPage();
    /*  Stat these paths off the GUI thread, ask the catalog, and report the stale ones.
        Shared by both passes so the freshness question cannot be asked two ways. */
    void dispatch(const QStringList &paths, bool forWholeSet);
    /*  The filters both passes apply to a candidate row, in proxy-row terms. Returns an
        empty string for a row not worth asking about. */
    QString pathToVerify(int sfRow, qint64 now, qint64 expiry) const;
    bool scopeIsHydrated() const;

    DataModel *dm;
    Metadata *metadata;
    QTimer settle;

    /*  path -> when it was last verified, on the same monotonic clock. */
    QElapsedTimer clock;
    QHash<QString, qint64> verifiedAt;

    /*  One pass at a time. A second pass launched while the first is in flight would
        stat the same window again and race it to the same conclusion. */
    bool inFlight = false;

    /*  THE WHOLE-SET SWEEP. wholeSetAt is the next proxy row to collect, or -1 when no
        sweep is running; wholeSet paces the pages. */
    QTimer wholeSet;
    int wholeSetAt = -1;
    int wholeSetStale = 0;          // reported across the sweep, for the probe line

    /*  A page is big enough that the per-page overhead is nothing beside 2,000 stats, and
        small enough that collecting it on the GUI thread is not a stall. */
    static constexpr int kWholeSetPage = 2000;
    /*  How long after the load before the sweep starts. The icons the index could not
        serve are still being read by MetaRead at that point, and 41,000 stats across the
        same volumes would be competing with them for nothing. */
    static constexpr int kWholeSetStartMs = 3000;
    /*  Between pages: long enough to let the GUI breathe, short enough that the sweep is
        over in seconds rather than minutes. */
    static constexpr int kWholeSetPaceMs = 25;
};

#endif // SCROLLVERIFY_H
