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
}

void ScrollVerify::reset()
{
    if (G::isLogger) G::log("ScrollVerify::reset");
    settle.stop();
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
        /*  A row still being read is not yet an answer about anything. */
        if (dm->sf->index(sfRow, G::MetadataStatusColumn).data().toInt() != G::MetaLoaded)
            continue;
        /*  A row whose file is on an unmounted volume or is gone cannot be stat'd
            usefully -- and on an absent network volume the attempt is not cheap. The
            availability pass has already said so; take its word for it. */
        if (dm->sf->index(sfRow, G::AvailabilityColumn).data().toInt()
                != int(Catalog::Availability::Present))
            continue;

        const QString fPath = dm->sf->index(sfRow, 0).data(G::PathRole).toString();
        if (fPath.isEmpty()) continue;
        const auto it = verifiedAt.constFind(fPath);
        if (it != verifiedAt.cend() && now - *it < expiry) continue;
        paths << fPath;
    }
    if (paths.isEmpty()) return;

    /*  Marked verified NOW, before the answer comes back, so a settle that lands while
        the pass is in flight does not queue the same window again. A path that turns out
        stale is re-read, which re-stamps it anyway. */
    for (const QString &p : paths) verifiedAt.insert(p, now);

    inFlight = true;
    Metadata *md = metadata;
    QThreadPool::globalInstance()->start([this, paths, md]{
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
            qDebug().noquote() << "[PERF] scroll-in verify" << paths.size()
                               << "rows: stat" << statMs << "ms + catalog"
                               << (t.elapsed() - statMs) << "ms (pool thread)  stale ="
                               << stale.size();
        }

        QMetaObject::invokeMethod(this, [this, stale]{
            inFlight = false;
            if (stale.isEmpty()) return;
            QStringList out;
            out.reserve(stale.size());
            for (const QString &p : stale) out << p;
            emit rowsAreStale(out);
        }, Qt::QueuedConnection);
    });
}
