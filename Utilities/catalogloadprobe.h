#ifndef CATALOGLOADPROBE_H
#define CATALOGLOADPROBE_H

#include <QtCore>

/*
    TEMPORARY -- HOW LONG DOES SELECTING THE CATALOG ACTUALLY TAKE, AND WHERE DOES IT GO?

    Selecting Catalog is one gesture and about a dozen stages, spread over four threads
    and three files, and the only numbers available until now were the [PERF] lines --
    each of which times ONE stage and none of which relate to the wall clock the user is
    watching. This records the whole run as a sequence of labelled segments and prints
    them, so "the delay" can be attributed rather than guessed at.

    IT IS A STOPWATCH WITH LAPS, not a profiler. begin() starts it, each mark() closes
    the previous segment and opens the next, end() closes the last one and prints the
    table. A segment is therefore WALL CLOCK between two call sites -- it includes
    whatever else the event loop did in between, which is the point: the user waits for
    wall clock, not for CPU.

    IT IS ONLY EVER RUNNING DURING A CATALOG LOAD. begin() is called from the Catalog
    scope switch alone, and mark() is a no-op when nothing is running, so the marks that
    sit in shared code (MW::folderChangeCompleted, BuildFilters::applyOps) cost one
    relaxed atomic load on a folder load and print nothing.

    THE SPAN IS THE ONE THE USER SEES: from the click that switches scope (1) to the
    moment the load is over. "Over" is TWO events, not one, and they can arrive in either
    order -- the loupe replaces the message pane when the image cache has the first image
    (6), and the filters panel stops being stale when the build completes (5) -- so the
    run ends at the LATER of them and the summary says which one it was waiting for. The
    availability pass and the background catalog commit run past that and are deliberately
    outside: they are not what anybody is waiting for.

    TO REMOVE: delete this file and `grep -rn "CatLoad::"` -- every call site is one line.
*/

namespace CatLoad {

struct Run {
    QMutex mutex;
    std::atomic<bool> running{false};
    QElapsedTimer total;
    QElapsedTimer step;
    QVector<QPair<QString, qint64>> laps;
    QStringList pending;        // the finish keys not yet arrived
    bool loading = false;       // the load itself has started; see cancel()
};

inline Run &run()
{
    static Run r;
    return r;
}

/*  Close the open segment and record it. Caller holds the mutex. */
inline void lap(Run &r, const QString &label)
{
    const qint64 ms = r.step.elapsed();
    r.laps.append({label, ms});
    r.step.restart();
    qDebug().noquote()
        << QString("[CATLOAD] %1 %2 ms   (t+%3 ms)")
               .arg(label, -44).arg(ms, 7).arg(r.total.elapsed(), 7);
}

inline void begin(const QString &what)
{
    Run &r = run();
    QMutexLocker lock(&r.mutex);
    /*  A run left open -- a search that loaded nothing, an aborted load -- is discarded
        rather than continued, so the next one is not reported as taking the time the
        user spent looking at the previous one. */
    r.laps.clear();
    r.pending = QStringList{"image", "filters"};
    r.loading = false;
    r.total.start();
    r.step.start();
    r.running.store(true, std::memory_order_relaxed);
    qDebug().noquote() << "[CATLOAD] ===== begin:" << what;
}

/*  AN EVENT THAT OVERLAPS THE TIMELINE RATHER THAN DIVIDING IT -- a worker thread
    starting or finishing, a background scan, a queued pass. It prints when it happened
    and closes NO segment, because the work it names ran alongside whatever the GUI
    thread was doing rather than after it. Using mark() for these would subtract their
    time from the segment they overlap and make both numbers fiction. */
inline void note(const QString &label)
{
    Run &r = run();
    if (!r.running.load(std::memory_order_relaxed)) return;
    QMutexLocker lock(&r.mutex);
    if (!r.running.load(std::memory_order_relaxed)) return;
    qDebug().noquote()
        << QString("[CATLOAD]    . %1           (t+%2 ms)")
               .arg(label, -44).arg(r.total.elapsed(), 7);
}

inline void mark(const QString &label)
{
    Run &r = run();
    if (!r.running.load(std::memory_order_relaxed)) return;
    QMutexLocker lock(&r.mutex);
    if (!r.running.load(std::memory_order_relaxed)) return;
    lap(r, label);
}

/*  One of the two things the run is waiting for has happened. The FIRST of them closes
    its own segment and is reported; the SECOND ends the run. A key that arrives twice
    (the loupe is switched to from more than one place) is ignored after the first. */
inline void finish(const QString &key, const QString &label)
{
    Run &r = run();
    if (!r.running.load(std::memory_order_relaxed)) return;
    QMutexLocker lock(&r.mutex);
    if (!r.running.load(std::memory_order_relaxed)) return;
    if (!r.pending.removeOne(key)) return;

    lap(r, label);
    if (!r.pending.isEmpty()) {
        qDebug().noquote() << "[CATLOAD]       still waiting for:" << r.pending.join(", ");
        return;
    }

    const qint64 tot = r.total.elapsed();
    r.running.store(false, std::memory_order_relaxed);

    qDebug().noquote() << "[CATLOAD] ----- summary (longest first)";
    QVector<QPair<QString, qint64>> sorted = r.laps;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const QPair<QString, qint64> &a, const QPair<QString, qint64> &b)
                     { return a.second > b.second; });
    for (const auto &l : sorted) {
        if (l.second == 0) continue;
        qDebug().noquote()
            << QString("[CATLOAD] %1 %2 ms  %3%")
                   .arg(l.first, -44).arg(l.second, 7)
                   .arg(100.0 * l.second / qMax(qint64(1), tot), 5, 'f', 1);
    }
    qDebug().noquote()
        << QString("[CATLOAD] ===== TOTAL  scope selected -> images shown AND filters "
                "built  %1 ms").arg(tot);
}

/*  AN EVENT THAT HAPPENS AFTER THE RUN HAS CLOSED, and still wants its place on the
    timeline. The whole point of the work this probe measures is that the load now ENDS
    before the icon pass and the verification sweep do -- so the events that matter most
    for what is left arrive when note() and mark() have already gone quiet, and were being
    silently dropped. This prints against the same clock, which is still valid, and says
    plainly that it is after the fact. */
inline void late(const QString &label)
{
    Run &r = run();
    QMutexLocker lock(&r.mutex);
    if (!r.total.isValid()) return;
    qDebug().noquote()
        << QString("[CATLOAD] (after) %1 (t+%2 ms)")
               .arg(label, -42).arg(r.total.elapsed(), 7);
}

/*  PAST HERE THE RUN IS REAL -- MW::loadCatalogScope has committed to replacing the
    model. It is what makes cancel() safe: FilterPanel::runSearch is called again while
    the load is in flight (the dock being shown re-runs it), finds the result unchanged,
    loads nothing, and would otherwise cancel the run it is standing in the middle of. */
inline void loadStarted()
{
    Run &r = run();
    if (!r.running.load(std::memory_order_relaxed)) return;
    QMutexLocker lock(&r.mutex);
    r.loading = true;
}

/*  A mark that belongs to the search, not the load, so a re-run of the search while the
    load is under way does not drop a second copy of it into the middle of the timeline. */
inline void markEarly(const QString &label)
{
    Run &r = run();
    if (!r.running.load(std::memory_order_relaxed)) return;
    QMutexLocker lock(&r.mutex);
    if (!r.running.load(std::memory_order_relaxed) || r.loading) return;
    lap(r, label);
}

/*  The run did not happen after all (the search matched nothing, or the set was too big
    to auto-load). Stop the clock without printing a total that means nothing. */
inline void cancel(const QString &why)
{
    Run &r = run();
    if (!r.running.load(std::memory_order_relaxed)) return;
    QMutexLocker lock(&r.mutex);
    if (r.loading) return;      // a re-run of the search, not this run's fate
    r.running.store(false, std::memory_order_relaxed);
    qDebug().noquote() << "[CATLOAD] ===== cancelled:" << why;
}

}  // namespace CatLoad

#endif // CATALOGLOADPROBE_H
