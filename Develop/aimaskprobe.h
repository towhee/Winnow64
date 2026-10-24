#ifndef AIMASKPROBE_H
#define AIMASKPROBE_H

/*
    AUTO-MASK PROBE (2026-10-23). An intermittent: a just-created Select Subject mask
    shows no overlay and its scope's edits change nothing, yet leaving the image and
    coming back heals it. Every cache that sits between the model result and the screen
    refuses to keep a mask built without its reference (buildMaskBuffer's refsReady,
    the fold-prefix cache, DevelopStackCache::dropMask), and every render tick retries
    the build -- so either the reference keeps failing to build, keeps being evicted, or
    is built under a different key than the one read. This records which.

    Always on, and cheap: only OUTCOMES are logged (a cache hit is just counted), and a
    run of identical lines collapses to one with a repeat count -- the "ref missing"
    line in buildMaskBuffer fires on every tick while the bug is showing. Thread-safe:
    buildMaskBuffer runs on the off-thread settle render as well as the GUI thread.

    Read it in Help > Diagnostics > Develop diagnostics ("AUTO-MASK PROBE"), taken
    BEFORE leaving the image -- leaving it is what heals the symptom.

    Header-only, no Q_OBJECT, so no build-system entry is needed.
*/

#include <QDateTime>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <deque>

namespace AiMaskProbe {

struct Line { QString text; int repeats = 1; QString first, last; };

inline QMutex &mutex() { static QMutex m; return m; }
inline std::deque<Line> &lines() { static std::deque<Line> d; return d; }
inline int &cachedHits() { static int n = 0; return n; }

inline void note(const QString &text)
{
    constexpr size_t kMax = 120;
    const QString now = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QMutexLocker lk(&mutex());
    std::deque<Line> &d = lines();
    if (!d.empty() && d.back().text == text) {
        ++d.back().repeats;
        d.back().last = now;
        return;
    }
    d.push_back(Line{text, 1, now, now});
    while (d.size() > kMax) d.pop_front();
}

inline void countCachedHit() { QMutexLocker lk(&mutex()); ++cachedHits(); }

inline QString dump()
{
    QMutexLocker lk(&mutex());
    QString s;
    s += QString("  cached-hit early-outs (all auto masks) = %1\n").arg(cachedHits());
    if (lines().empty()) s += "  (no events)\n";
    for (const Line &l : lines()) {
        s += "  " + l.first + "  " + l.text;
        if (l.repeats > 1)
            s += QString("   [x%1, last %2]").arg(l.repeats).arg(l.last);
        s += "\n";
    }
    return s;
}

} // namespace AiMaskProbe

#endif // AIMASKPROBE_H
