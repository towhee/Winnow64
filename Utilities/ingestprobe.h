#ifndef INGESTPROBE_H
#define INGESTPROBE_H

#include <QtCore>
#include <QElapsedTimer>
#include <atomic>

/*
    WHAT AN INGEST SESSION ACTUALLY COSTS, AND WHAT IT LOSES.

    Ingest is the one workflow that runs every subsystem at once against the slowest
    possible storage: thousands of files on a card, three views, MetaRead's reader pool,
    the ImageCache decoder pool, a sidecar write per classification, and a copy at the
    end. The three complaints it produces -- a blank loupe, a stutter, a crash -- are all
    reported the same way ("sometimes"), and none of the existing probes can tell them
    apart:

      o The scroll and icon probes (G::isPerfProbe) measure the LOAD path. During a cull
        the load is finished; the time is going somewhere else.
      o The GUI stall watchdog says a stall happened and how long it was, not what was
        running.
      o ImageCache::diagnostics() is a snapshot. A blank loupe is an EVENT -- the image
        was not there when it was needed and arrived (or did not) later -- and a snapshot
        taken afterwards shows a perfectly healthy cache.

    So this records EVENTS, on the two paths the user is actually driving:

      SELECTION   arrow key / click -> fileSelectionChange -> loupe.  Was the image in
                  the cache when the loupe asked for it? If not, did the cache signal
                  ever arrive to repair it, and how long did the user look at nothing?
      EDIT        1-5, 0-5, P.  How much of the keypress was the sidecar, how much was
                  repainting icons, how much was the filter rebuild?

    plus a small set of counters for the ways a decoded image can be silently thrown
    away -- an instance clash drops the notification that paints the loupe, and nothing
    anywhere says so.

    THE COST WHEN DISARMED IS ONE RELAXED ATOMIC LOAD per hook, and every hook site is
    written as `if (G::isIngestProbe) ...` so even that is skipped. Nothing is allocated,
    no file is opened, and no timer runs until Arm(true).

    THREADING. The event buffers are GUI-thread only: BeginSelection, EndSelection,
    BeginEdit and the report are all called from there. The counters (NoteDroppedCacheSignal,
    NoteDiscardedDecode, NoteHollowCacheHit) are hit from the ImageCache and decoder
    threads and are plain atomics -- no buffer, no lock, no ordering requirement.
*/

class IngestProbe
{
public:
    static IngestProbe &Instance();

    /*  Arming starts the clock and clears everything. Disarming keeps the buffers so the
        report can still be read after the fact. */
    void Arm(bool on);
    bool IsArmed() const { return armed; }
    void Reset();

    /*  Where the loupe got its pixels for one selection. Miss is the interesting one:
        the image was not in the cache, so the loupe was blanked and the user is waiting
        on a decode that may or may not report back. */
    enum Loupe { Unset, Cached, Miss, Interim, Video, NoView };

    /* ---- selection path (GUI thread) ---- */
    void BeginSelection(int sfRow, const QString &fPath);
    void NoteLoupe(Loupe outcome);
    void MarkSelection(const char *phase);
    void EndSelection();
    /*  ImageCache reported fPath cached and MW repainted the loupe with it. Closes the
        most recent Miss for that path. */
    void NoteLoupeRepair(const QString &fPath);

    /* ---- classification edits (GUI thread) ---- */
    void BeginEdit(const QString &what, int n);
    void MarkEdit(const char *phase);
    void EndEdit();

    /* ---- counters (any thread) ---- */
    /*  DataModel::setCached refused the notification because the instance had moved on.
        The image IS in the cache; the loupe is simply never told, which is one of the two
        ways a selection can stay blank forever. */
    void NoteDroppedCacheSignal();
    /*  ImageCache::okToCache refused a finished decode. The work was done and discarded,
        so the row goes back on the queue and the wait starts again. */
    void NoteDiscardedDecode();
    /*  icd->contains() said yes and the value came back null -- the unguarded read in
        ImageView::loadImage losing a race with a trim. The loupe paints an empty pixmap
        and reports success. */
    void NoteHollowCacheHit();
    /*  DataModel::newInstance(). Every bump invalidates in-flight decodes and reader
        work; during a cull they come from filter changes provoked by classification
        keys, not from folder changes. */
    void NoteInstanceBump(const QString &src);
    /*  One pass of MW::updateImageCacheStatus's target-range loop: rows is how many
        cells it painted. Called once per image entering or leaving the cache. */
    void NoteCacheStatusPaint(int rows);
    /*  The GUI stall watchdog fired. */
    void NoteStall(qint64 ms);

    /*  WHAT HELD THE EVENT LOOP.

        A pause the user can see but the phase timers cannot: guiMs stays flat while the
        GAP between selections jumps, which says the time went somewhere OUTSIDE
        fileSelectionChange -- a queued slot, a deferred layout, a repaint. Naming it by
        reading the code is guesswork, so QtSingleApplication::notify times every event
        DELIVERY and reports the slow ones here, named by receiver class and event type.

        Nesting is the point rather than a problem: an inner delivery is timed too, so the
        innermost long-running leaf is charged as well as its caller. */
    void NoteEventCost(qint64 ms, const char *cls, const QString &objName, int evType);

    /*  WHICH SLOT, when the event type is not enough.

        A queued call arrives as QEvent::MetaCall on its receiver and Qt does not expose
        which method it carries -- so "MW(MW)/MetaCall took 290 ms" names the object and
        stops. Scope closes that gap the only way the public API allows: the handful of MW
        entry points that a worker thread can invoke each declare their own name.

        One relaxed atomic load when disarmed. Declare it as the first line of the slot;
        it is timed by its destructor, so early returns are covered. */
    class Scope
    {
    public:
        explicit Scope(const char *name);
        ~Scope();
    private:
        const char *slot;
        QElapsedTimer timer;
        bool on;
    };
    /*  Called by Scope. Not for general use. */
    void NoteSlotCost(const char *slot, qint64 ms);

    QString Report() const;
    /*  The report to stderr, for a run with no menu to reach it from -- a headless
        --ingestprobe --selftest sweep, or an ordinary quit that leaves the summary in
        console.txt alongside the [INGEST] lines it explains. Silent when disarmed. */
    void DumpReport() const;

private:
    IngestProbe() = default;
    Q_DISABLE_COPY_MOVE(IngestProbe)

    struct SelEvent {
        qint64 atMs = 0;            // ms since arm
        qint64 gapMs = 0;           // since the previous selection: the navigation rate
        qint64 guiMs = 0;           // wall time inside fileSelectionChange
        int    sfRow = -1;
        QString name;               // file name only; the path is in the buffer key
        QString path;
        Loupe  loupe = Unset;
        qint64 repairMs = -1;       // Miss only: -1 not yet, -2 abandoned, else the wait
        QString phases;             // "loadImage=12.4 cachePos=3.1"
    };

    struct HogEvent {
        qint64 atMs = 0;
        qint64 ms = 0;
        QString what;               // "IconView(Thumbnails)/Paint"
    };

    struct EditEvent {
        qint64 atMs = 0;
        qint64 totalMs = 0;
        int    n = 0;               // images in the selection
        QString what;               // "colour", "rating", "pick"
        QString phases;
    };

    static QString Pct(const QList<qint64> &sorted, int pct);
    static QString MsList(const QList<qint64> &v);

    std::atomic<bool> armed{false};

    QElapsedTimer clock;            // since Arm
    QElapsedTimer selTimer;         // the selection being timed
    QElapsedTimer editTimer;
    qint64 selPhaseNs = 0;
    qint64 editPhaseNs = 0;
    qint64 prevSelAtMs = -1;
    bool   inSelection = false;
    bool   inEdit = false;

    SelEvent  cur;                  // selection under construction
    EditEvent curEdit;
    QStringList curPhases;
    QStringList curEditPhases;

    QVector<SelEvent> sels;
    QVector<EditEvent> edits;
    QVector<HogEvent> hogs;
    QMap<QString, QPair<int, qint64>> hogTally;   // what -> (count, total ms)
    QMap<QString, QPair<int, qint64>> slotTally;  // MW slot -> (count, total ms)
    static constexpr int maxSels = 600;
    static constexpr int maxEdits = 300;
    static constexpr int maxHogs = 300;
    /*  25 ms is about one and a half frames: below it nothing is visible, above it a
        burst of them is what a person calls a pause. */
    static constexpr qint64 kHogMs = 25;


    /*  Misses waiting on a repair, newest last. Indexes into sels; a path can appear
        more than once, so the newest match is the one closed. */
    QVector<int> openMisses;

    std::atomic<int> droppedCacheSignals{0};
    std::atomic<int> discardedDecodes{0};
    std::atomic<int> hollowCacheHits{0};
    std::atomic<int> cacheStatusPaints{0};
    std::atomic<qint64> cacheStatusRows{0};
    std::atomic<int> stalls{0};
    std::atomic<qint64> stallMsTotal{0};
    std::atomic<qint64> stallMsMax{0};

    mutable QMutex bumpMutex;       // instanceBumps is written from the GUI thread only,
    QMap<QString,int> instanceBumps;// but read by Report() which may be called anywhere
};

#endif // INGESTPROBE_H
