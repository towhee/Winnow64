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

    /*  A new set is loading. Forget every answer -- they were about other images. */
    void reset();

signals:
    /*  These paths are indexed, were served from the index, and no longer match their
        files. Delivered on the GUI thread. */
    void rowsAreStale(const QStringList &paths);

private:
    void runPass();
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
};

#endif // SCROLLVERIFY_H
