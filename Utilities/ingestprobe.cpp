#include "Utilities/ingestprobe.h"
#include "Utilities/utilities.h"
#include "Main/global.h"

#include <QFileInfo>
#include <QCoreApplication>
#include <QEvent>
#include <QMetaEnum>
#include <algorithm>

/*
    See ingestprobe.h for what this is for and why the existing probes could not answer
    the questions it answers.

    LIVE LINES AS WELL AS A REPORT. A cull is a long session and the report is read at the
    end, but the thing being chased happens in the middle -- so anything abnormal is also
    printed as it happens, prefixed [INGEST], next to the [PERF] lines the stall watchdog
    writes. Ordinary selections print nothing: a line per arrow key would bury the twenty
    that matter under two thousand that do not.
*/

namespace {
/*  Print a selection line only when it is worth reading: the loupe did not come out of
    the cache, or the keypress cost more than a person would call instant. */
constexpr qint64 kSelNoisyMs = 120;
constexpr qint64 kEditNoisyMs = 150;
}

IngestProbe &IngestProbe::Instance()
{
    static IngestProbe probe;
    return probe;
}

void IngestProbe::Arm(bool on)
{
    if (on) {
        Reset();
        clock.start();
        armed.store(true, std::memory_order_relaxed);
        qDebug().noquote() << "[INGEST] probe armed";
    }
    else {
        armed.store(false, std::memory_order_relaxed);
        qDebug().noquote() << "[INGEST] probe disarmed after"
                           << (clock.isValid() ? clock.elapsed() : 0) << "ms";
    }
}

void IngestProbe::Reset()
{
    sels.clear();
    edits.clear();
    openMisses.clear();
    curPhases.clear();
    curEditPhases.clear();
    inSelection = false;
    inEdit = false;
    prevSelAtMs = -1;
    droppedCacheSignals = 0;
    discardedDecodes = 0;
    hollowCacheHits = 0;
    hogs.clear();
    hogTally.clear();
    slotTally.clear();
    cacheStatusPaints = 0;
    cacheStatusRows = 0;
    stalls = 0;
    stallMsTotal = 0;
    stallMsMax = 0;
    {
        QMutexLocker lock(&bumpMutex);
        instanceBumps.clear();
    }
    clock.restart();
}

/* ------------------------------------------------------------------ selection path */

void IngestProbe::BeginSelection(int sfRow, const QString &fPath)
{
    if (!armed.load(std::memory_order_relaxed)) return;

    /*  A selection can begin while one is still open: fileSelectionChange can re-enter
        (a filter change recovers the selection, autoAdvance runs inside an edit). The
        outer one is closed rather than dropped, so its time is still reported -- it just
        includes the nested one, which is the truth about what the keypress cost. */
    if (inSelection) EndSelection();

    cur = SelEvent();
    cur.atMs = clock.elapsed();
    cur.gapMs = (prevSelAtMs < 0) ? -1 : (cur.atMs - prevSelAtMs);
    prevSelAtMs = cur.atMs;
    cur.sfRow = sfRow;
    cur.path = fPath;
    cur.name = QFileInfo(fPath).fileName();
    curPhases.clear();
    selPhaseNs = 0;
    inSelection = true;
    selTimer.start();
}

void IngestProbe::NoteLoupe(Loupe outcome)
{
    if (!armed.load(std::memory_order_relaxed) || !inSelection) return;
    cur.loupe = outcome;
}

void IngestProbe::MarkSelection(const char *phase)
{
    if (!armed.load(std::memory_order_relaxed) || !inSelection) return;
    const qint64 now = selTimer.nsecsElapsed();
    const double ms = (now - selPhaseNs) / 1.0e6;
    selPhaseNs = now;
    if (ms >= 1.0)
        curPhases << QString("%1=%2").arg(phase).arg(QString::number(ms, 'f', 1));
}

void IngestProbe::EndSelection()
{
    if (!armed.load(std::memory_order_relaxed) || !inSelection) return;
    inSelection = false;

    cur.guiMs = selTimer.elapsed();
    cur.phases = curPhases.join(" ");

    /*  A Miss that is still open when the NEXT selection begins is abandoned: the user
        moved on, so whatever arrives later repairs an image nobody is looking at. That is
        the blank-loupe complaint in its worst form, and it is counted separately from a
        miss that was repaired late. */
    for (int i : std::as_const(openMisses)) {
        if (i >= 0 && i < sels.size() && sels[i].repairMs == -1) {
            sels[i].repairMs = -2;
            qDebug().noquote() << "[INGEST] loupe NEVER repaired  row =" << sels[i].sfRow
                               << sels[i].name;
        }
    }
    openMisses.clear();

    if (sels.size() >= maxSels) sels.remove(0, sels.size() - maxSels + 1);
    sels.append(cur);
    if (cur.loupe == Miss) openMisses.append(sels.size() - 1);

    if (cur.loupe != Cached || cur.guiMs >= kSelNoisyMs) {
        static const char *name[] = {"unset", "cached", "MISS", "interim", "video", "noview"};
        qDebug().noquote() << "[INGEST] sel row =" << cur.sfRow
                           << " gap =" << cur.gapMs << "ms"
                           << " gui =" << cur.guiMs << "ms"
                           << " loupe =" << name[cur.loupe]
                           << " " << cur.phases;
    }
}

void IngestProbe::NoteLoupeRepair(const QString &fPath)
{
    if (!armed.load(std::memory_order_relaxed)) return;

    /*  Newest first: the same image can be revisited, and the wait being closed is the
        one the user is sitting in front of. */
    for (int k = openMisses.size() - 1; k >= 0; --k) {
        const int i = openMisses.at(k);
        if (i < 0 || i >= sels.size()) continue;
        if (sels[i].path != fPath) continue;
        sels[i].repairMs = clock.elapsed() - sels[i].atMs;
        qDebug().noquote() << "[INGEST] loupe repaired  row =" << sels[i].sfRow
                           << " after" << sels[i].repairMs << "ms" << sels[i].name;
        openMisses.remove(k);
        return;
    }
}

/* ------------------------------------------------------------------- edits */

void IngestProbe::BeginEdit(const QString &what, int n)
{
    if (!armed.load(std::memory_order_relaxed)) return;
    curEdit = EditEvent();
    curEdit.atMs = clock.elapsed();
    curEdit.what = what;
    curEdit.n = n;
    curEditPhases.clear();
    editPhaseNs = 0;
    inEdit = true;
    editTimer.start();
}

void IngestProbe::MarkEdit(const char *phase)
{
    if (!armed.load(std::memory_order_relaxed) || !inEdit) return;
    const qint64 now = editTimer.nsecsElapsed();
    const double ms = (now - editPhaseNs) / 1.0e6;
    editPhaseNs = now;
    if (ms >= 1.0)
        curEditPhases << QString("%1=%2").arg(phase).arg(QString::number(ms, 'f', 1));
}

void IngestProbe::EndEdit()
{
    if (!armed.load(std::memory_order_relaxed) || !inEdit) return;
    inEdit = false;
    curEdit.totalMs = editTimer.elapsed();
    curEdit.phases = curEditPhases.join(" ");

    if (edits.size() >= maxEdits) edits.remove(0, edits.size() - maxEdits + 1);
    edits.append(curEdit);

    if (curEdit.totalMs >= kEditNoisyMs)
        qDebug().noquote() << "[INGEST] edit" << curEdit.what
                           << " n =" << curEdit.n
                           << " total =" << curEdit.totalMs << "ms"
                           << " " << curEdit.phases;
}

/* ------------------------------------------------------------------- counters */

void IngestProbe::NoteDroppedCacheSignal()
{
    if (!armed.load(std::memory_order_relaxed)) return;
    droppedCacheSignals.fetch_add(1, std::memory_order_relaxed);
}

void IngestProbe::NoteDiscardedDecode()
{
    if (!armed.load(std::memory_order_relaxed)) return;
    discardedDecodes.fetch_add(1, std::memory_order_relaxed);
}

void IngestProbe::NoteHollowCacheHit()
{
    if (!armed.load(std::memory_order_relaxed)) return;
    hollowCacheHits.fetch_add(1, std::memory_order_relaxed);
    qWarning().noquote() << "[INGEST] hollow cache hit: contains() true, image null";
}

void IngestProbe::NoteInstanceBump(const QString &src)
{
    if (!armed.load(std::memory_order_relaxed)) return;
    QMutexLocker lock(&bumpMutex);
    instanceBumps[src]++;
}

void IngestProbe::NoteCacheStatusPaint(int rows)
{
    if (!armed.load(std::memory_order_relaxed)) return;
    cacheStatusPaints.fetch_add(1, std::memory_order_relaxed);
    cacheStatusRows.fetch_add(rows, std::memory_order_relaxed);
}

void IngestProbe::NoteStall(qint64 ms)
{
    if (!armed.load(std::memory_order_relaxed)) return;
    stalls.fetch_add(1, std::memory_order_relaxed);
    stallMsTotal.fetch_add(ms, std::memory_order_relaxed);
    qint64 prev = stallMsMax.load(std::memory_order_relaxed);
    while (ms > prev &&
           !stallMsMax.compare_exchange_weak(prev, ms, std::memory_order_relaxed))
    {}
}

/* ------------------------------------------------------------------- event hog */

IngestProbe::Scope::Scope(const char *name)
    : slot(name), on(G::isIngestProbe.load(std::memory_order_relaxed))
{
    if (on) timer.start();
}

IngestProbe::Scope::~Scope()
{
    if (!on) return;
    IngestProbe::Instance().NoteSlotCost(slot, timer.elapsed());
}

void IngestProbe::NoteSlotCost(const char *slot, qint64 ms)
{
    if (!armed.load(std::memory_order_relaxed)) return;
    const QString key = QString::fromLatin1(slot);
    auto &t = slotTally[key];
    t.first += 1;
    t.second += ms;
    if (ms >= kHogMs)
        qDebug().noquote() << "[INGEST] slot" << ms << "ms " << key;
}

void IngestProbe::NoteEventCost(qint64 ms, const char *cls, const QString &objName,
                                int evType)
{
    if (!armed.load(std::memory_order_relaxed)) return;

    static const QMetaEnum evEnum = QMetaEnum::fromType<QEvent::Type>();
    const char *evName = evEnum.valueToKey(evType);

    QString what = QString::fromLatin1(cls ? cls : "?");
    if (!objName.isEmpty()) what += "(" + objName + ")";
    what += "/";
    what += evName ? QString::fromLatin1(evName) : QString::number(evType);

    auto &t = hogTally[what];
    t.first += 1;
    t.second += ms;

    if (hogs.size() >= maxHogs) hogs.remove(0, hogs.size() - maxHogs + 1);
    hogs.append({clock.isValid() ? clock.elapsed() : 0, ms, what});

    qDebug().noquote() << "[INGEST] event held the loop" << ms << "ms " << what;
}

/* ------------------------------------------------------------------- report */

QString IngestProbe::Pct(const QList<qint64> &sorted, int pct)
{
    if (sorted.isEmpty()) return "-";
    int i = (sorted.size() - 1) * pct / 100;
    return QString::number(sorted.at(i));
}

QString IngestProbe::MsList(const QList<qint64> &v)
{
    QList<qint64> s = v;
    std::sort(s.begin(), s.end());
    return QString("n=%1  p50=%2  p90=%3  max=%4")
        .arg(s.size()).arg(Pct(s, 50)).arg(Pct(s, 90)).arg(Pct(s, 100));
}

void IngestProbe::DumpReport() const
{
    if (!armed.load(std::memory_order_relaxed)) return;
    const QByteArray r = Report().toLocal8Bit();
    fprintf(stderr, "%s\n", r.constData());
    fflush(stderr);
}

QString IngestProbe::Report() const
{
    QString reportString;
    QTextStream rpt(&reportString);
    rpt.setFieldAlignment(QTextStream::AlignLeft);

    rpt << "\n";
    rpt << Utilities::centeredRptHdr('=', "Ingest Probe");
    rpt << "\n";

    if (sels.isEmpty() && edits.isEmpty() && !armed.load(std::memory_order_relaxed)) {
        rpt << "\nThe ingest probe has not been armed this session.\n"
            << "Help > Diagnostics > Ingest probe (arm) starts it, then work normally\n"
            << "and come back here.  It measures the selection -> loupe path and the\n"
            << "classification keys, and costs nothing while disarmed.\n\n";
        return reportString;
    }

    const qint64 upMs = clock.isValid() ? clock.elapsed() : 0;
    rpt << "\n";
    rpt << "armed                    = " << (armed.load(std::memory_order_relaxed)
                                             ? "true" : "false (buffers kept)") << "\n";
    rpt << "elapsed                  = " << upMs / 1000.0 << " s\n";
    rpt << "selections recorded      = " << sels.size()
        << (sels.size() >= maxSels ? "  (buffer full, oldest dropped)" : "") << "\n";
    rpt << "edits recorded           = " << edits.size() << "\n";

    /* ---------------------------------------------------------- navigation */
    QList<qint64> guiMs, gapMs;
    int cached = 0, miss = 0, interim = 0, video = 0, noview = 0;
    QList<qint64> repairMs;
    int abandoned = 0;
    for (const SelEvent &e : sels) {
        guiMs << e.guiMs;
        if (e.gapMs >= 0) gapMs << e.gapMs;
        switch (e.loupe) {
        case Cached:  ++cached; break;
        case Miss:    ++miss;
                      if (e.repairMs >= 0) repairMs << e.repairMs;
                      else if (e.repairMs == -2) ++abandoned;
                      break;
        case Interim: ++interim; break;
        case Video:   ++video; break;
        case NoView:  ++noview; break;
        default: break;
        }
    }

    rpt << "\n";
    rpt << Utilities::centeredRptHdr('-', "Navigation");
    rpt << "\n";
    rpt << "gui time per selection   = " << MsList(guiMs) << "  ms\n";
    rpt << "gap between selections   = " << MsList(gapMs) << "  ms\n";

    rpt << "\n";
    rpt << "loupe from cache         = " << cached << "\n";
    rpt << "loupe MISS (blanked)     = " << miss << "\n";
    rpt << "  repaired               = " << repairMs.size()
        << (repairMs.isEmpty() ? "" : QString("   %1  ms").arg(MsList(repairMs))) << "\n";
    rpt << "  abandoned (never shown)= " << abandoned << "\n";
    rpt << "  still open             = " << (miss - repairMs.size() - abandoned) << "\n";
    rpt << "loupe interim (develop)  = " << interim << "\n";
    rpt << "video                    = " << video << "\n";
    rpt << "no image view            = " << noview << "\n";

    /*  THE SLOWEST TEN, WITH THEIR PHASES. A percentile says a stutter happened; only the
        phase split says which of the dozen things fileSelectionChange does was the
        stutter. */
    QVector<const SelEvent*> worst;
    for (const SelEvent &e : sels) worst.append(&e);
    std::sort(worst.begin(), worst.end(),
              [](const SelEvent *a, const SelEvent *b){ return a->guiMs > b->guiMs; });
    rpt << "\n" << "slowest selections:\n";
    for (int i = 0; i < worst.size() && i < 10; ++i) {
        const SelEvent *e = worst.at(i);
        rpt << "  " << QString::number(e->guiMs).rightJustified(6) << " ms  row "
            << QString::number(e->sfRow).rightJustified(6) << "  "
            << e->name.leftJustified(28) << "  " << e->phases << "\n";
    }

    /*  IS THE PAUSE PERIODIC, AND WHAT IS ITS PERIOD?

        The complaint is "a short pause approximately every 35 images", and a percentile
        cannot answer it -- p90 is the same number whether the slow ones are evenly spaced
        or all in a clump. THE GAP, NOT guiMs: a pause the user sees while
        fileSelectionChange stays fast is time spent OUTSIDE it, which is the whole point
        of measuring both. The threshold is relative to the run's own typing rate (2.5x
        the median gap), because a key held down produces a steady rate and anything that
        stands out against it is what is being asked about.

        Printed as the DISTANCE IN SELECTIONS from one long gap to the next. A constant
        distance is a period, and its value names the mechanism -- a screenful of
        thumbnails, an icon chunk, a decoder pool cycle are all different numbers. */
    if (gapMs.size() >= 8) {
        QList<qint64> sortedGaps = gapMs;
        std::sort(sortedGaps.begin(), sortedGaps.end());
        const qint64 medianGap = sortedGaps.at(sortedGaps.size() / 2);
        const qint64 hiccupMs = qMax<qint64>(medianGap * 5 / 2, medianGap + 25);

        QVector<int> at;            // index into sels of each long gap
        for (int i = 0; i < sels.size(); ++i)
            if (sels.at(i).gapMs >= hiccupMs) at.append(i);

        rpt << "\n";
        rpt << "median gap               = " << medianGap << " ms   (the held-key rate)\n";
        rpt << "long gaps (>= " << hiccupMs << " ms)  = " << at.size() << "\n";

        if (at.size() >= 1) {
            QList<qint64> periods;
            for (int k = 1; k < at.size(); ++k) periods << (at.at(k) - at.at(k - 1));
            QList<qint64> sortedPeriods = periods;
            std::sort(sortedPeriods.begin(), sortedPeriods.end());
            if (!periods.isEmpty())
                rpt << "  selections between them= " << MsList(periods)
                    << "   <- a steady number here IS the period\n";

            /*  THE PERIOD IN MILLISECONDS AS WELL AS IN SELECTIONS, because at a steady
                key-repeat rate those are the same observation and they have opposite
                causes. A constant number of SELECTIONS points at something driven by row
                position -- a screenful of thumbnails, a cache range, a chunk boundary. A
                constant number of MILLISECONDS points at a timer, and no amount of
                staring at the navigation code would ever find it. */
            QList<qint64> periodMs;
            for (int k = 1; k < at.size(); ++k)
                periodMs << (sels.at(at.at(k)).atMs - sels.at(at.at(k - 1)).atMs);
            if (!periodMs.isEmpty())
                rpt << "  ms between them        = " << MsList(periodMs)
                    << "   <- steady HERE instead means a timer\n";

            /*  AND WHAT RAN DURING THE GAP. Each long gap is a window of wall time with
                nothing of ours in it; the event hog records what the loop was delivering.
                This is the line that names the cause rather than describing the symptom. */
            rpt << "  each one:\n";
            int shown = 0;
            for (int k = 0; k < at.size(); ++k) {
                const SelEvent &e = sels.at(at.at(k));
                rpt << "    +" << QString::number(k ? at.at(k) - at.at(k - 1) : 0).rightJustified(4)
                    << " sel   gap " << QString::number(e.gapMs).rightJustified(6)
                    << " ms   gui " << QString::number(e.guiMs).rightJustified(5)
                    << " ms   row " << QString::number(e.sfRow).rightJustified(6)
                    << "   " << e.phases << "\n";
                const qint64 from = e.atMs - e.gapMs;
                int listed = 0;
                for (const HogEvent &h : hogs) {
                    if (h.atMs < from || h.atMs > e.atMs) continue;
                    rpt << "            during: " << QString::number(h.ms).rightJustified(5)
                        << " ms  " << h.what << "\n";
                    if (++listed >= 4) { rpt << "            ...\n"; break; }
                }
                if (++shown >= 25) { rpt << "    ...\n"; break; }
            }
        }
    }

    /*  Every abandoned miss, named. This is the blank-loupe bug reduced to a list of
        images it happened on -- which is what makes it reproducible. */
    if (abandoned) {
        rpt << "\n" << "abandoned (loupe blank, cache never reported back):\n";
        int shown = 0;
        for (const SelEvent &e : sels) {
            if (e.loupe != Miss || e.repairMs != -2) continue;
            rpt << "  row " << QString::number(e.sfRow).rightJustified(6) << "  "
                << e.name << "\n";
            if (++shown >= 20) { rpt << "  ...\n"; break; }
        }
    }

    /* ---------------------------------------------------------- edits */
    rpt << "\n";
    rpt << Utilities::centeredRptHdr('-', "Classification Edits");
    rpt << "\n";
    QMap<QString, QList<qint64>> byKind;
    for (const EditEvent &e : edits) byKind[e.what] << e.totalMs;
    if (byKind.isEmpty()) rpt << "none recorded\n";
    for (auto it = byKind.cbegin(); it != byKind.cend(); ++it)
        rpt << it.key().leftJustified(24) << " = " << MsList(it.value()) << "  ms\n";

    QVector<const EditEvent*> worstEdit;
    for (const EditEvent &e : edits) worstEdit.append(&e);
    std::sort(worstEdit.begin(), worstEdit.end(),
              [](const EditEvent *a, const EditEvent *b){ return a->totalMs > b->totalMs; });
    if (!worstEdit.isEmpty()) rpt << "\n" << "slowest edits:\n";
    for (int i = 0; i < worstEdit.size() && i < 10; ++i) {
        const EditEvent *e = worstEdit.at(i);
        rpt << "  " << QString::number(e->totalMs).rightJustified(6) << " ms  "
            << e->what.leftJustified(10) << " n=" << QString::number(e->n).rightJustified(4)
            << "  " << e->phases << "\n";
    }

    /* ---------------------------------------------------------- discards */
    rpt << "\n";
    rpt << Utilities::centeredRptHdr('-', "Work Thrown Away");
    rpt << "\n";
    rpt << "dropped setCached        = " << droppedCacheSignals.load()
        << "   (image cached, loupe never told -- instance clash)\n";
    rpt << "discarded decodes        = " << discardedDecodes.load()
        << "   (decode finished, result refused -- instance clash)\n";
    rpt << "hollow cache hits        = " << hollowCacheHits.load()
        << "   (contains() true, image null -- trim raced the read)\n";

    rpt << "\n" << "datamodel instance bumps (each one invalidates in-flight work):\n";
    {
        QMutexLocker lock(&bumpMutex);
        if (instanceBumps.isEmpty()) rpt << "  none\n";
        for (auto it = instanceBumps.cbegin(); it != instanceBumps.cend(); ++it)
            rpt << "  " << QString::number(it.value()).rightJustified(6) << "  "
                << it.key() << "\n";
    }

    /* ---------------------------------------------------------- gui cost */
    rpt << "\n";
    rpt << Utilities::centeredRptHdr('-', "GUI Thread");
    rpt << "\n";
    const int paints = cacheStatusPaints.load();
    rpt << "cache status paints      = " << paints << "\n";
    rpt << "  cells painted          = " << cacheStatusRows.load()
        << (paints ? QString("   (%1 per paint)")
                         .arg(cacheStatusRows.load() / qMax(1, paints)) : QString()) << "\n";
    rpt << "gui stalls               = " << stalls.load()
        << "   total " << stallMsTotal.load() << " ms"
        << "   worst " << stallMsMax.load() << " ms\n";

    /*  WHAT HELD THE EVENT LOOP, by receiver and event type. This is the line that names
        a pause the phase timers cannot see, because it charges the time to the event
        delivery that actually spent it rather than to the keypress that was waiting. */
    rpt << "\n" << "single event deliveries over " << kHogMs << " ms:\n";
    if (hogTally.isEmpty()) rpt << "  none\n";
    else {
        QVector<QPair<QString, QPair<int, qint64>>> byTotal;
        for (auto it = hogTally.cbegin(); it != hogTally.cend(); ++it)
            byTotal.append({it.key(), it.value()});
        std::sort(byTotal.begin(), byTotal.end(),
                  [](const auto &a, const auto &b){ return a.second.second > b.second.second; });
        rpt << "     count    total     mean   what\n";
        for (int i = 0; i < byTotal.size() && i < 15; ++i) {
            const auto &e = byTotal.at(i);
            rpt << "  " << QString::number(e.second.first).rightJustified(8)
                << QString::number(e.second.second).rightJustified(9) << " ms"
                << QString::number(e.second.second / qMax(1, e.second.first)).rightJustified(7)
                << " ms   " << e.first << "\n";
        }
    }

    /*  AND WHICH SLOT. A queued call is a MetaCall on its receiver and nothing more --
        the tally above can say MW spent 290 ms and cannot say doing what. These are the
        MW entry points a worker thread can invoke, each timed by name. */
    rpt << "\n" << "MW slots reached from a worker thread:\n";
    if (slotTally.isEmpty()) rpt << "  none called\n";
    else {
        QVector<QPair<QString, QPair<int, qint64>>> byTotal;
        for (auto it = slotTally.cbegin(); it != slotTally.cend(); ++it)
            byTotal.append({it.key(), it.value()});
        std::sort(byTotal.begin(), byTotal.end(),
                  [](const auto &a, const auto &b){ return a.second.second > b.second.second; });
        rpt << "     count    total     mean   slot\n";
        for (int i = 0; i < byTotal.size() && i < 20; ++i) {
            const auto &e = byTotal.at(i);
            rpt << "  " << QString::number(e.second.first).rightJustified(8)
                << QString::number(e.second.second).rightJustified(9) << " ms"
                << QString::number(e.second.second / qMax(1, e.second.first)).rightJustified(7)
                << " ms   " << e.first << "\n";
        }
    }

    rpt << "\n\n";
    return reportString;
}
