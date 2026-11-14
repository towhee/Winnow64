#ifndef PIPELINECOUNTERS_H
#define PIPELINECOUNTERS_H

#include <QHash>
#include <QMutex>
#include <QString>
#include <atomic>

/*
    SESSION COUNTS OF THE PIPELINE'S SELF-REPAIRS (Help > Diagnostics > Pipeline sync
    counters, Documentation 4.4.19). Each one is a moment the pipeline caught itself out
    of step and recovered: a worker result that arrived for a model instance that had
    moved on, an image that ran out of decode attempts, a decode that landed after its
    row had left the queue, a loupe the watchdog had to repair. None is an error on its
    own; a count that climbs during ordinary browsing says where to look.

    Session totals, never reset: ImageCache's own lifetime counters restart at every
    folder load, which hides a slow climb across folders. Any thread. Header-only and
    outside Main/global.h, which every file includes.
*/
namespace PipelineCounters {

// okToCache: a successful decode whose row had been trimmed from toCache meanwhile
inline std::atomic<quint64> lateDecodes{0};
// okToDecode: distinct images that reached "Max attempts reached" (once per load each)
inline std::atomic<quint64> attemptCapImages{0};
// MW::loupeWatchdog outcomes
inline std::atomic<quint64> watchdogRepaints{0};   // cached but not shown: repainted
inline std::atomic<quint64> watchdogReposts{0};    // not cached yet: position re-posted
inline std::atomic<quint64> watchdogGaveUp{0};     // still not shown after its tries

/*  Instance clashes by site: a result stamped with an instance the model has left. The
    site is the function that dropped it (and, for DataModel's writers, who sent it). */
inline QMutex clashMutex;
inline QHash<QString, quint64> clashBySite;

inline void noteClash(const QString &site)
{
    QMutexLocker lock(&clashMutex);
    ++clashBySite[site];
}

inline QHash<QString, quint64> clashes()
{
    QMutexLocker lock(&clashMutex);
    return clashBySite;
}

}  // namespace PipelineCounters

#endif // PIPELINECOUNTERS_H
