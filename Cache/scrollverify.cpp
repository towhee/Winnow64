#include "Cache/scrollverify.h"

#include <QDebug>
#include <QFileInfo>
#include <QList>
#include <QSet>
#include <QThreadPool>

#include "Cache/catalog.h"
#include "Datamodel/datamodel.h"
#include "Main/global.h"
#include "Metadata/indexmetadata.h"
#include "Metadata/metadata.h"

ScrollVerify::ScrollVerify(DataModel *dm, Metadata *metadata, QObject *parent)
    : QObject(parent), dm(dm), metadata(metadata)
{
    clock.start();
    settle.setSingleShot(true);
    settle.setInterval(kSettleMs);
    connect(&settle, &QTimer::timeout, this, &ScrollVerify::runPass);

    wholeSet.setSingleShot(true);
    connect(&wholeSet, &QTimer::timeout, this, &ScrollVerify::runWholeSetPage);
}

void ScrollVerify::reset()
{
    if (G::isLogger) G::log("ScrollVerify::reset");
    settle.stop();
    wholeSet.stop();
    wholeSetAt = -1;
    wholeSetStale = 0;
    verifiedAt.clear();
}

void ScrollVerify::viewChanged()
{
    if (!G::useScrollInVerify) return;
    if (G::isInitializing) return;
    if (!scopeIsHydrated()) return;
    settle.start();                     // restart: the pass runs when scrolling stops
}

bool ScrollVerify::scopeIsHydrated() const
{
/*
    ONLY ROWS THAT WERE FILLED FROM THE INDEX ARE WORTH VERIFYING.

    A folder scope read every row from its file, so the row IS the file and a stat could
    only tell us what we already know. Two shapes are hydrated instead: a Catalog scope
    whose rows came back in the query that found them (ScopeRequest::rows), and any load
    at all while G::useIndexMetadata is on, where the Reader takes the row from the
    catalog whenever the catalog can answer.

    This is the same test MW::folderChangeCompleted uses to decide there is nothing to
    commit, and for the same underlying fact -- so the two must not drift: what that one
    skips committing is what this one is responsible for.
*/
    if (dm == nullptr) return false;
    if (G::useIndexMetadata) return true;
    const ScopeRequest &req = dm->scopeRequest();
    return req.scope == G::Scope::Catalog && !req.rows.isEmpty();
}

QString ScrollVerify::pathToVerify(int sfRow, qint64 now, qint64 expiry) const
{
/*
    The row filters, in one place, because the whole-set sweep and the scroll pass must
    ask about the same rows for the same reasons -- two copies would drift and the two
    passes would disagree about the same picture.
*/
    /*  A row still being read is not yet an answer about anything. */
    if (dm->sf->index(sfRow, G::MetadataStatusColumn).data().toInt() != G::MetaLoaded)
        return QString();
    /*  A row whose file is on an unmounted volume or is gone cannot be stat'd
        usefully -- and on an absent network volume the attempt is not cheap. The
        availability pass has already said so; take its word for it. */
    if (dm->sf->index(sfRow, G::AvailabilityColumn).data().toInt()
            != int(Catalog::Availability::Present))
        return QString();

    const QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();
    if (fPath.isEmpty()) return QString();
    const auto it = verifiedAt.constFind(fPath);
    if (it != verifiedAt.cend() && now - *it < expiry) return QString();
    return fPath;
}

void ScrollVerify::dispatch(const QStringList &paths, bool forWholeSet)
{
/*
    Stat the paths off the GUI thread, ask the catalog which of them have moved, and
    report those. Both passes come through here.
*/
    inFlight = true;
    Metadata *md = metadata;
    QThreadPool::globalInstance()->start([this, paths, md, forWholeSet]{
        /*  THE STAT IS THE POINT AND IT IS WHY THIS RUNS OFF THE GUI THREAD. One per
            image plus one per sidecar -- IndexMetadata::candidate does both, and it is
            the same candidate the loader builds, so the freshness question is asked in
            exactly the terms the index answers it in. */
        QElapsedTimer t;
        const bool probe = G::isPerfProbe;
        if (probe) t.start();

        QList<CatalogRow> cands;
        cands.reserve(paths.size());
        for (const QString &p : paths)
            cands.append(IndexMetadata::candidate(QFileInfo(p), md));
        const qint64 statMs = probe ? t.elapsed() : 0;

        const QSet<QString> stale = Catalog::instance().outOfDate(cands);

        if (probe) {
            qDebug().noquote() << "[PERF] verify" << paths.size()
                               << "rows: stat" << statMs << "ms + catalog"
                               << (t.elapsed() - statMs) << "ms (pool thread)  stale ="
                               << stale.size();
        }

        QMetaObject::invokeMethod(this, [this, stale, forWholeSet]{
            inFlight = false;
            if (stale.isEmpty()) return;
            /*  Only the sweep's own findings, or its summary would be inflated by every
                scroll pass that happened to run while it was walking. */
            if (forWholeSet) wholeSetStale += stale.size();
            QStringList out;
            out.reserve(stale.size());
            for (const QString &p : stale) out << p;
            emit rowsAreStale(out);
        }, Qt::QueuedConnection);
    });
}

void ScrollVerify::verifyWholeSet()
{
/*
    Start (or restart) the background sweep of every loaded row. See the header.
*/
    if (!G::useScrollInVerify) return;
    if (G::isInitializing) return;
    if (!scopeIsHydrated()) return;
    if (G::isLogger || G::isFlowLogger) G::log("ScrollVerify::verifyWholeSet");

    wholeSetAt = 0;
    wholeSetStale = 0;
    wholeSet.start(kWholeSetStartMs);
}

void ScrollVerify::runWholeSetPage()
{
/*
    One page of the sweep: collect on the GUI thread, stat on a pool thread, schedule the
    next page. GUI thread.
*/
    if (wholeSetAt < 0) return;

    /*  ANY OF THESE MEANS THE SET THIS SWEEP WAS ABOUT IS NO LONGER LOADED. Stopping
        rather than pausing is deliberate: a new load calls reset() and then starts its
        own sweep, so there is nothing here worth resuming. */
    if (!G::useScrollInVerify || dm == nullptr || dm->sf == nullptr
        || G::stop || dm->abort || !scopeIsHydrated()) {
        wholeSetAt = -1;
        return;
    }

    /*  THE SCROLL PASS OUTRANKS THE SWEEP, and a load outranks both. Deferring rather
        than skipping keeps the sweep's place: the page is collected when the slot is
        free, not abandoned. */
    if (inFlight || G::isModifyingDatamodel) {
        wholeSet.start(kWholeSetPaceMs);
        return;
    }

    const int rows = dm->sf->rowCount();
    if (wholeSetAt >= rows) {
        if (G::isPerfProbe)
            qDebug().noquote() << "[PERF] whole-set verify finished:" << rows
                               << "rows, stale =" << wholeSetStale;
        wholeSetAt = -1;
        return;
    }

    const qint64 now = clock.elapsed();
    const qint64 expiry = qint64(kReverifySecs) * 1000;
    const int end = qMin(rows, wholeSetAt + kWholeSetPage);

    QStringList paths;
    paths.reserve(end - wholeSetAt);
    for (int sfRow = wholeSetAt; sfRow < end; ++sfRow) {
        const QString p = pathToVerify(sfRow, now, expiry);
        if (!p.isEmpty()) paths << p;
    }
    wholeSetAt = end;

    if (!paths.isEmpty()) {
        /*  Remembered as verified before the answer comes back, exactly as the scroll
            pass does -- which is also what makes the two passes cheap together: a row
            this sweep has just asked about is one the next scroll settle skips. The cost
            is one hash entry per loaded row, ~10 MB at 41,000 rows, cleared by reset(). */
        for (const QString &p : paths) verifiedAt.insert(p, now);
        dispatch(paths, /*forWholeSet*/ true);
    }

    wholeSet.start(kWholeSetPaceMs);
}

void ScrollVerify::runPass()
{
/*
    Collect the visible rows worth asking about, then hand the question to a pool
    thread. GUI thread: it reads the model.
*/
    if (!G::useScrollInVerify || inFlight || dm == nullptr || dm->sf == nullptr) return;
    if (!scopeIsHydrated()) return;

    const int rows = dm->sf->rowCount();
    const int first = qMax(0, dm->firstVisibleIcon);
    const int last  = qMin(rows - 1, dm->lastVisibleIcon);
    if (last < first) return;

    const qint64 now = clock.elapsed();
    const qint64 expiry = qint64(kReverifySecs) * 1000;

    QStringList paths;
    for (int sfRow = first; sfRow <= last; ++sfRow) {
        const QString p = pathToVerify(sfRow, now, expiry);
        if (!p.isEmpty()) paths << p;
    }
    if (paths.isEmpty()) return;

    /*  Marked verified NOW, before the answer comes back, so a settle that lands while
        the pass is in flight does not queue the same window again. A path that turns out
        stale is re-read, which re-stamps it anyway. */
    for (const QString &p : paths) verifiedAt.insert(p, now);

    dispatch(paths, /*forWholeSet*/ false);
}
