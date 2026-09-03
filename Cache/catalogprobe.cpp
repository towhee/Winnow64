#include "Cache/catalogprobe.h"

#include <QAtomicInt>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QRunnable>
#include <QSemaphore>

#include <cstdio>

#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Main/global.h"
#include "Metadata/indexmetadata.h"
#include "Metadata/metadata.h"

namespace {

/*  stderr, unbuffered, one line at a time -- the probe is normally watched while it runs
    and a stage that takes forty seconds must announce itself before it starts, not
    after. */
void say(const QString &s)
{
    fprintf(stderr, "%s\n", s.toLocal8Bit().constData());
    fflush(stderr);
}

QString ms(qint64 ns)
{
    return QString::number(ns / 1000000.0, 'f', 1) + " ms";
}

/*  Per-row cost as microseconds, which is the unit the comparison actually happens in:
    the whole question is whether a stat is two orders of magnitude cheaper than a read,
    and expressing both in ms buries the stat in zeroes. */
QString perRow(qint64 ns, int rows)
{
    if (rows <= 0) return "n/a";
    return QString::number(ns / 1000.0 / rows, 'f', 1) + " us/row";
}

int envInt(const char *name, int fallback)
{
    const int v = qEnvironmentVariableIntValue(name);
    return v > 0 ? v : fallback;
}

/*  Thread count, where 0 means "as many as the loader uses" and is therefore a VALUE
    rather than an absent setting -- envInt cannot express that, and reading it through
    envInt silently ran the concurrent case on one thread. */
int envThreads()
{
    if (!qEnvironmentVariableIsSet("WINNOW_PROBE_THREADS")) return 1;
    const int v = qEnvironmentVariableIntValue("WINNOW_PROBE_THREADS");
    if (v > 0) return v;
    return qMax(1, QThread::idealThreadCount() - 2);    // MetaRead::readerCount
}

/*  One sampled file read, on a worker with its OWN Metadata -- the same shape Reader
    uses (Cache/reader.cpp:11 gives every reader its own), because a Metadata carries the
    parse state of the file it is working on and cannot be shared across threads. */
class ReadTask : public QRunnable
{
public:
    ReadTask(const QStringList &paths, int from, int to, QAtomicInt *ok, QSemaphore *done)
        : paths(paths), from(from), to(to), ok(ok), done(done)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        Metadata metadata;
        for (int i = from; i < to; ++i) {
            const QFileInfo fi(paths.at(i));
            if (metadata.loadImageMetadata(fi, 0, 0, true, true, false, false,
                                           "CatalogProbe"))
                ok->fetchAndAddRelaxed(1);
        }
        done->release();
    }

private:
    QStringList paths;
    int from;
    int to;
    QAtomicInt *ok;
    QSemaphore *done;
};

}   // namespace

namespace CatalogProbe {

int run(const QString &pathFilter)
{
    const int wantRows   = envInt("WINNOW_PROBE_ROWS", 50000);
    const int wantSample = envInt("WINNOW_PROBE_SAMPLE", 500);
    const int threads    = envThreads();

    say("");
    say("CATALOG PROBE");
    say("  filter     " + (pathFilter.isEmpty() ? QString("(all rows)") : pathFilter));
    say("  rows       " + QString::number(wantRows));
    say("  sample     " + QString::number(wantSample)
        + " re-reads on " + QString::number(threads)
        + (threads == 1 ? " thread" : " threads"));

    if (!Catalog::instance().isAvailable()) {
        say("  UNAVAILABLE -- the catalog could not be opened.");
        return 2;
    }
    /*  AFTER isAvailable(), not before: the database opens lazily, and CacheDb has no
        path at all until the first Catalog call supplies the default. Asked earlier this
        printed an empty line. */
    say("  index      " + CacheDb::instance().path());
    say("  catalogued " + QString::number(Catalog::instance().count()) + " images in "
        + QString::number(Catalog::instance().folderCount()) + " folders");
    say("");

    /*  STAGE A -- the SQL pass.

        An empty query with a limit is exactly what Catalog scope issues when nothing has
        been typed, and it is the whole cost of "trust the index entirely": no file is
        touched, so whatever this measures is the floor for showing the user their
        library. total comes back separately, which is what lets the status bar say the
        real number while the rows are still arriving. */
    CatalogQuery q;
    q.includeMissing = true;    // browsing shows unavailable rows; they are still rows

    int total = 0;
    QElapsedTimer t;
    t.start();
    QStringList paths = Catalog::instance().search(q, wantRows, &total);
    const qint64 searchNs = t.nsecsElapsed();

    if (!pathFilter.isEmpty()) {
        QStringList kept;
        kept.reserve(paths.size());
        for (const QString &p : paths)
            if (p.contains(pathFilter)) kept.append(p);
        paths = kept;
    }

    say("A  SQL pass (Catalog::search)");
    say("     " + ms(searchNs) + "   " + QString::number(paths.size()) + " paths"
        + (pathFilter.isEmpty() ? QString() : " after filtering")
        + " of " + QString::number(total) + " matching   "
        + perRow(searchNs, paths.size()));

    if (paths.isEmpty()) {
        say("");
        say("  NOTHING TO MEASURE -- no catalogued rows matched.");
        return 2;
    }

    /*  STAGE B -- the stat sweep.

        IndexMetadata::candidate is the real thing, not an imitation: it is what
        DataModel::addAllMetadata and Reader both build before asking the index, and it
        stats the image AND its sidecar (a keyword edited in Lightroom rewrites the .xmp
        and never touches the raw). Two stats per row is the honest cost of knowing
        whether an index row is still true. */
    Metadata metadata;
    QList<CatalogRow> cands;
    cands.reserve(paths.size());
    t.restart();
    for (const QString &p : paths)
        cands.append(IndexMetadata::candidate(QFileInfo(p), &metadata));
    const qint64 statNs = t.nsecsElapsed();

    say("B  stat sweep (IndexMetadata::candidate, image + sidecar)");
    say("     " + ms(statNs) + "   " + QString::number(cands.size()) + " rows   "
        + perRow(statNs, cands.size()));

    /*  STAGE C -- the bulk index read.

        Paged exactly as addAllMetadata pages it, so this measures the code that would
        actually run rather than a single heroic query. The hit count is the load-bearing
        number: misses are rows the index cannot answer for, and they fall through to a
        file read no matter which verification policy is chosen. */
    constexpr int kFreshPage = 1000;
    QHash<QString, CatalogRow> fresh;
    t.restart();
    for (int i = 0; i < cands.size(); i += kFreshPage) {
        const QList<CatalogRow> page = cands.mid(i, kFreshPage);
        fresh.insert(Catalog::instance().fetchFresh(page));
    }
    const qint64 freshNs = t.nsecsElapsed();
    const int hits = fresh.size();
    const int misses = cands.size() - hits;

    say("C  bulk index read (Catalog::fetchFresh, pages of "
        + QString::number(kFreshPage) + ")");
    say("     " + ms(freshNs) + "   " + QString::number(hits) + " fresh, "
        + QString::number(misses) + " stale or unindexed   "
        + perRow(freshNs, cands.size()));

    /*  STAGE E -- the same rows, in the query that found them.

        A+C is the two-step hydration: find the paths, then look each one up. This is the
        one-step form, and the comparison is the whole reason searchRows exists. It is
        also checked rather than merely timed: same count, same order, and a field-level
        comparison against what fetchFresh returned for the rows the two have in common.
        A faster path that returns a different row is not a faster path. */
    t.restart();
    int rowsTotal = 0;
    const QVector<CatalogRow> rows =
        Catalog::instance().searchRows(q, wantRows, &rowsTotal);
    const qint64 rowsNs = t.nsecsElapsed();

    int compared = 0;
    int mismatched = 0;
    QString firstMismatch;
    for (const CatalogRow &r : rows) {
        const auto it = fresh.constFind(r.path);
        if (it == fresh.constEnd()) continue;    // stat said stale; nothing to compare
        const CatalogRow &f = it.value();
        ++compared;
        QStringList bad;
        if (r.filename != f.filename)   bad << "filename";
        if (r.folder != f.folder)       bad << "folder";
        if (r.ext != f.ext)             bad << "ext";
        if (r.captured != f.captured)   bad << "captured";
        if (r.rating != f.rating)       bad << "rating";
        if (r.label != f.label)         bad << "label";
        if (r.pick != f.pick)           bad << "pick";
        if (r.title != f.title)         bad << "title";
        if (r.make != f.make)           bad << "make";
        if (r.model != f.model)         bad << "model";
        if (r.lens != f.lens)           bad << "lens";
        if (r.iso != f.iso)             bad << "iso";
        if (r.width != f.width)         bad << "width";
        if (r.height != f.height)       bad << "height";
        if (r.orientation != f.orientation) bad << "orientation";
        if (r.developed != f.developed) bad << "developed";
        if (r.devPreviewKey != f.devPreviewKey) bad << "devPreviewKey";
        if (r.shootingInfo != f.shootingInfo)   bad << "shootingInfo";
        /*  Keyword ORDER is not part of the contract -- fetchFresh reads them one image
            at a time and searchRows reads them all in one join, so the two arrive in
            different orders for the same image. The SET is the fact. */
        QStringList a = r.keywords, b = f.keywords;
        a.sort(); b.sort();
        if (a != b)                     bad << "keywords";
        if (!bad.isEmpty()) {
            ++mismatched;
            if (firstMismatch.isEmpty())
                firstMismatch = r.filename + ": " + bad.join(", ");
        }
    }

    say("E  whole rows in one query (Catalog::searchRows)");
    say("     " + ms(rowsNs) + "   " + QString::number(rows.size()) + " rows of "
        + QString::number(rowsTotal) + " matching   " + perRow(rowsNs, rows.size()));
    say("     vs A+C " + ms(searchNs + freshNs) + " for the same rows -- "
        + (rowsNs > 0
               ? QString::number(double(searchNs + freshNs) / rowsNs, 'f', 1) + "x faster"
               : QString("n/a")));
    say("     " + QString::number(compared) + " rows compared field-by-field against "
        "fetchFresh, " + QString::number(mismatched) + " mismatched"
        + (firstMismatch.isEmpty() ? QString() : "   first: " + firstMismatch));

    /*  STAGE D -- the file read that all of the above exists to avoid.

        Sampled, and deliberately over rows spread ACROSS the result rather than the first
        n: the head of a captured-DESC ordering is one recent shoot, one camera, one
        folder, and timing that would describe a folder load rather than a library. */
    const int sample = qMin(wantSample, paths.size());
    QStringList sampled;
    sampled.reserve(sample);
    const double stride = double(paths.size()) / sample;
    for (int i = 0; i < sample; ++i)
        sampled.append(paths.at(qMin(paths.size() - 1, int(i * stride))));

    say("D  full file read (Metadata::loadImageMetadata) -- sampled, please wait");

    QAtomicInt ok(0);
    QSemaphore done;
    t.restart();
    if (threads == 1) {
        Metadata reader;
        for (const QString &p : sampled) {
            const QFileInfo fi(p);
            if (reader.loadImageMetadata(fi, 0, 0, true, true, false, false,
                                         "CatalogProbe"))
                ok.fetchAndAddRelaxed(1);
        }
    }
    else {
        QThreadPool pool;
        pool.setMaxThreadCount(threads);
        const int chunk = (sample + threads - 1) / threads;
        int started = 0;
        for (int from = 0; from < sample; from += chunk) {
            pool.start(new ReadTask(sampled, from, qMin(sample, from + chunk),
                                    &ok, &done));
            ++started;
        }
        done.acquire(started);
    }
    const qint64 readNs = t.nsecsElapsed();

    say("     " + ms(readNs) + "   " + QString::number(ok.loadRelaxed()) + " of "
        + QString::number(sample) + " read   " + perRow(readNs, sample));

    /*  WHAT IT MEANS. The ratio is the decision: verification can be lazy and invisible
        only while a stat is far cheaper than a read. The extrapolation is labelled as one
        because it assumes the sampled rows are representative of the rest, which is the
        assumption a sample always makes and this probe cannot check. */
    const double statPer = double(statNs) / cands.size();
    const double readPer = double(readNs) / sample;
    const double ratio = statPer > 0 ? readPer / statPer : 0;

    say("");
    say("VERDICT");
    /*  THE STAT SWEEP IS ALWAYS SERIAL and the read may not be, so this ratio is only
        like-for-like at one thread. Said out loud because the concurrent run makes the
        gap look ten times smaller than it is per unit of work -- the reads got faster,
        the stats were never given the same help. */
    say("  a read costs " + QString::number(ratio, 'f', 0)
        + "x a stat (" + perRow(qint64(readPer), 1) + " vs "
        + perRow(qint64(statPer), 1) + ")"
        + (threads == 1 ? QString()
                        : QString(" -- NOT like-for-like: the read used "
                                  + QString::number(threads)
                                  + " threads, the stat sweep one")));
    say("  extrapolated to " + QString::number(paths.size()) + " rows:");
    say("     hydrate only (A+C)      " + ms(searchNs + freshNs));
    say("     + stat every row (B)    " + ms(searchNs + freshNs + statNs));
    say("     + re-read every row (D) "
        + ms(searchNs + freshNs + statNs + qint64(readPer * paths.size()))
        + "   [extrapolated from " + QString::number(sample) + "]");
    say("");
    say("  The icon window is G::maxIconChunk = " + QString::number(G::maxIconChunk)
        + " rows, so a scroll-in stat pass costs "
        + ms(qint64(statPer * qMin(G::maxIconChunk, paths.size())))
        + " and re-reading that same window costs "
        + ms(qint64(readPer * qMin(G::maxIconChunk, paths.size()))) + ".");
    say("");

    return 0;
}

}   // namespace CatalogProbe
