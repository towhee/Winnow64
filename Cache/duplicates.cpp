#include "Cache/duplicates.h"
#include "Cache/catalog.h"
#include "Datamodel/duplicateok.h"
#include "ImageFormats/Video/mov.h"
#include "Main/global.h"
#include "Metadata/metadata.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QThreadPool>
#include <QTimer>
#include <memory>

namespace {

/* How long to sleep between checks while giving way to a folder load. */
constexpr int kPauseSliceMs = 100;
/* Progress reports at most this often. */
constexpr qint64 kReportMs = 250;
/* An incremental check reports progress only after it has run this long. */
constexpr qint64 kQuietMs = 1000;
/*  Files decoded in parallel before their fingerprints are written. Small, because a
    full-size decode is ~100 MB of pixels at 24 MP: a batch is bounded by the pool, and
    the pool by kDecodeThreads. */
constexpr int kBatch = 12;
/* Debounces, GUI side. Rows arrive in commit-sized bursts during a scan or an ingest. */
constexpr int kKeysDebounceMs = 500;
constexpr int kRebuildDebounceMs = 300;
constexpr const char *kCheckedKey = "duplicates_checked";

/* The video formats MOV::durationMs can read (the ISO-BMFF family). */
const QSet<QString> &mvhdExts()
{
    static const QSet<QString> s = {"mov", "mp4", "m4v", "m4p", "3gp", "3g2"};
    return s;
}

int decodeThreads()
{
    return qBound(1, QThread::idealThreadCount() / 4, 3);
}

/*  The decode pool. Its own, not the global one: a full check would otherwise sit in
    front of every thumbnail and preview task queued behind it. 8 MB stacks, because a
    pool thread's default (512 KB on macOS) is what the raw parsers have overrun
    before. */
QThreadPool &decodePool()
{
    static QThreadPool *pool = [] {
        auto *p = new QThreadPool;
        p->setMaxThreadCount(decodeThreads());
        p->setStackSize(8 * 1024 * 1024);
        p->setThreadPriority(QThread::LowPriority);
        return p;
    }();
    return *pool;
}

/*  One Metadata per decode thread, created on that thread (CatalogScanner's rule: a
    Metadata is not shared between threads). */
Metadata *threadMetadata()
{
    thread_local std::unique_ptr<Metadata> md;
    if (!md) md = std::make_unique<Metadata>();
    return md.get();
}

/*  The image as Preview mode shows it, before orientation: a raw's embedded full-size
    JPEG, otherwise the file itself. A null image is a file that could not be decoded. */
QImage decodeForHash(const DuplicateRow &r)
{
    Metadata *md = threadMetadata();
    if (md->hasJpg.contains(r.ext)) {
        const QFileInfo fi(r.path);
        if (md->loadImageMetadata(fi, 0, G::dmInstance, true, false, false, false,
                                  "DuplicateChecker")) {
            const quint32 offset = md->m.offsetFull;
            const quint32 length = md->m.lengthFull;
            if (offset && length) {
                QFile f(r.path);
                if (f.open(QIODevice::ReadOnly) && f.seek(offset)) {
                    const QByteArray bytes = f.read(length);
                    QImage im;
                    if (bytes.size() == qsizetype(length) && im.loadFromData(bytes, "JPG"))
                        return im;
                }
            }
        }
    }
    QImageReader reader(r.path);
    reader.setAutoTransform(false);         // orientation is NOT part of the pixels
    reader.setDecideFormatFromContent(true);
    return reader.read();
}

/*  SHA-1 of the pixels, row by row so a scanline's padding is not part of it, with the
    size first so two images of the same bytes and a different shape cannot match. */
QByteArray hashPixels(const QImage &src)
{
    const QImage::Format want = src.hasAlphaChannel() ? QImage::Format_ARGB32
                                                      : QImage::Format_RGB32;
    const QImage im = src.format() == want ? src : src.convertToFormat(want);
    QCryptographicHash h(QCryptographicHash::Sha1);
    const qint32 dims[2] = {qint32(im.width()), qint32(im.height())};
    h.addData(QByteArrayView(reinterpret_cast<const char *>(dims), sizeof dims));
    const qsizetype rowBytes = qsizetype(im.width()) * 4;
    for (int y = 0; y < im.height(); ++y)
        h.addData(QByteArrayView(reinterpret_cast<const char *>(im.constScanLine(y)),
                                 rowBytes));
    return h.result();
}

bool needsFingerprint(const DuplicateRow &r)
{
    return r.pixelHash.isEmpty() && r.durationMs == DuplicateRow::kNoDuration;
}

}   // namespace

/* ---------------------------------------------------------------------------------
   DuplicateChecker
   --------------------------------------------------------------------------------- */

DuplicateChecker::DuplicateChecker(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<DuplicateProgress>("DuplicateProgress");
    qRegisterMetaType<DuplicateCounts>("DuplicateCounts");
    qRegisterMetaType<QHash<QString, DuplicateInfo>>("QHash<QString,DuplicateInfo>");
    qRegisterMetaType<QSet<QString>>("QSet<QString>");
    {
        /* The video list is Metadata's; read once here rather than per row. */
        Metadata md;
        for (const QString &e : std::as_const(md.videoFormats)) videoExts.insert(e);
    }
    moveToThread(&checkerThread);
    checkerThread.start(QThread::LowPriority);
}

DuplicateChecker::~DuplicateChecker()
{
    shutdown();
}

void DuplicateChecker::shutdown(int maxWaitMs)
{
    closing.store(true, std::memory_order_relaxed);
    stop();
    if (!checkerThread.isRunning()) return;
    checkerThread.quit();
    checkerThread.wait(QDeadlineTimer(maxWaitMs));
}

void DuplicateChecker::stop()
{
    abort.store(true, std::memory_order_relaxed);
}

bool DuplicateChecker::shouldPause() const
{
    /* CatalogScanner::shouldPause, for the same reasons: give way to what the user is
       waiting on, and treat G::stop as a pause, not an abort. */
    return G::isModifyingDatamodel || G::isLoadRunning || G::stop;
}

bool DuplicateChecker::waitWhilePaused()
{
    while (shouldPause()) {
        if (abort.load(std::memory_order_relaxed) || closing.load()) return false;
        QThread::msleep(kPauseSliceMs);
    }
    return !abort.load(std::memory_order_relaxed) && !closing.load();
}

int DuplicateChecker::fingerprint(QVector<DuplicateRow> rows, bool full)
{
/*
    Fingerprint the rows that need it, kBatch at a time on the decode pool, writing each
    batch as it completes so a stopped check keeps what it did. Returns rows written.
*/
    QVector<DuplicateRow> todo;
    for (const DuplicateRow &r : std::as_const(rows))
        if (needsFingerprint(r)) todo << r;
    if (todo.isEmpty()) return 0;

    QElapsedTimer clock;
    clock.start();
    qint64 lastReport = -kReportMs;
    qint64 pausedMs = 0;
    qint64 decodeMs = 0, hashMs = 0;        // summed over the pool, for the probe
    int images = 0, videos = 0, failed = 0, noDuration = 0;
    QMutex statMutex;

    int written = 0;
    DuplicateProgress p;
    p.total = int(todo.size());
    p.full = full;
    auto report = [&](bool force, bool paused) {
        const qint64 now = clock.elapsed();
        if (!full && now < kQuietMs) return;
        if (!force && now - lastReport < kReportMs) return;
        lastReport = now;
        p.paused = paused;
        const qint64 active = now - pausedMs;
        p.etaSecs = (p.done >= 20 && active > 2000)
                        ? double(active) / p.done * (p.total - p.done) / 1000.0
                        : -1;
        emit progress(p);
    };

    for (int from = 0; from < todo.size(); from += kBatch) {
        if (shouldPause()) {
            report(true, true);
            QElapsedTimer pt;
            pt.start();
            const bool go = waitWhilePaused();
            pausedMs += pt.elapsed();
            if (!go) break;
        }
        if (abort.load(std::memory_order_relaxed) || closing.load()) break;
        if (full && cancelFullGen.load(std::memory_order_relaxed) >= fullGen) break;

        const int to = qMin(int(todo.size()), from + kBatch);
        QVector<DuplicateRow> batch = todo.mid(from, to - from);
        for (DuplicateRow &r : batch) {
            if (videoExts.contains(r.ext)) {
                /* Two seeks, not worth a pool task. */
                r.durationMs = mvhdExts().contains(r.ext) ? MOV::durationMs(r.path) : -1;
                if (r.durationMs < 0) ++noDuration;
                ++videos;
                continue;
            }
            DuplicateRow *row = &r;
            decodePool().start([row, &decodeMs, &hashMs, &failed, &statMutex] {
                QElapsedTimer t;
                t.start();
                const QImage im = decodeForHash(*row);
                const qint64 d = t.elapsed();
                QByteArray h = im.isNull() ? DuplicateStatus::failedHash() : hashPixels(im);
                const qint64 hm = t.elapsed() - d;
                row->pixelHash = h;
                QMutexLocker lk(&statMutex);
                decodeMs += d;
                hashMs += hm;
                if (im.isNull()) ++failed;
            });
            ++images;
        }
        decodePool().waitForDone();
        written += Catalog::instance().setDuplicateFingerprints(batch);
        p.done = to;
        report(false, false);
    }
    report(true, false);

    if (G::isPerfProbe || full)
        qDebug().noquote() << "[Duplicates] fingerprinted" << written << "of" << todo.size()
                           << "candidates (" << images << "images," << videos << "videos)"
                           << "in" << clock.elapsed() << "ms  paused =" << pausedMs
                           << "ms  decode =" << decodeMs << "ms  hash =" << hashMs
                           << "ms (pool total," << decodeThreads() << "threads)"
                           << " undecodable =" << failed
                           << " videos without a duration =" << noDuration;
    return written;
}

void DuplicateChecker::checkAll(int gen, const QSet<QString> &okPaths)
{
    fullGen = gen;
    if (cancelFullGen.load(std::memory_order_relaxed) >= gen) {     // stopped while queued
        emit finished(true, true, 0);
        return;
    }
    abort.store(false, std::memory_order_relaxed);
    running.store(true, std::memory_order_relaxed);
    QElapsedTimer t;
    t.start();
    const QVector<DuplicateRow> rows = Catalog::instance().duplicateCandidates();
    const qint64 queryMs = t.elapsed();
    const int n = fingerprint(rows, true);
    const bool cancelled = abort.load(std::memory_order_relaxed)
                           || cancelFullGen.load(std::memory_order_relaxed) >= gen;
    if (!cancelled)
        Catalog::instance().setMetaValue(kCheckedKey,
                                         QString::number(QDateTime::currentSecsSinceEpoch()));
    qDebug().noquote() << "[Duplicates] full check:" << rows.size() << "candidates,"
                       << "query" << queryMs << "ms, total" << t.elapsed() << "ms"
                       << (cancelled ? "(stopped)" : "");
    rebuild(okPaths);
    running.store(false, std::memory_order_relaxed);
    emit finished(true, cancelled, n);
}

void DuplicateChecker::checkKeys(const QStringList &pathkeys, const QSet<QString> &okPaths)
{
    if (closing.load()) return;
    /*  A Stop pressed on a full check must not be inherited by the incremental check
        queued behind it, nor stop this one before it starts. */
    abort.store(false, std::memory_order_relaxed);
    running.store(true, std::memory_order_relaxed);
    const QVector<DuplicateRow> rows = Catalog::instance().duplicateCandidatesFor(pathkeys);
    const int n = fingerprint(rows, false);
    /* Rebuild even when nothing was decoded: a returning row changes the status. */
    rebuild(okPaths);
    running.store(false, std::memory_order_relaxed);
    emit finished(false, abort.load(std::memory_order_relaxed), n);
}

void DuplicateChecker::rebuild(const QSet<QString> &okPaths)
{
    if (closing.load()) return;
    QElapsedTimer t;
    t.start();
    const QVector<DuplicateRow> rows = Catalog::instance().duplicateFingerprinted();
    DuplicateCounts counts;
    const QHash<QString, DuplicateInfo> status = classifyDuplicates(rows, okPaths, &counts);
    if (G::isPerfProbe)
        qDebug().noquote() << "[PERF] Duplicates rebuild" << rows.size()
                           << "fingerprinted rows ->" << status.size() << "in groups in"
                           << t.elapsed() << "ms";
    emit rebuilt(status, counts);
}

/* ---------------------------------------------------------------------------------
   Duplicates
   --------------------------------------------------------------------------------- */

Duplicates &Duplicates::instance()
{
    static Duplicates *d = new Duplicates;      // lives for the app, like Catalog
    return *d;
}

void Duplicates::start()
{
    if (checker) return;
    checker = new DuplicateChecker;
    connect(checker, &DuplicateChecker::progress, this, &Duplicates::progress,
            Qt::QueuedConnection);
    connect(checker, &DuplicateChecker::rebuilt, this,
            [this](const QHash<QString, DuplicateInfo> &status, const DuplicateCounts &c) {
        const bool changed = status.size() != statusByPath.size()
                             || c.found != lastCounts.found
                             || c.okayed != lastCounts.okayed
                             || c.groups != lastCounts.groups
                             || [&] {
                                    for (auto it = status.cbegin(); it != status.cend(); ++it) {
                                        const DuplicateInfo o = statusByPath.value(it.key());
                                        if (o.status != it->status || o.twins != it->twins
                                            || o.group != it->group
                                            || o.developed != it->developed
                                            || o.versions != it->versions)
                                            return true;
                                    }
                                    return false;
                                }();
        statusByPath = status;
        pathsByGroup.clear();
        for (auto it = status.cbegin(); it != status.cend(); ++it)
            pathsByGroup[it->group].append(it.key());
        for (QStringList &paths : pathsByGroup) paths.sort();
        lastCounts = c;
        if (changed) emit statusChanged();
    }, Qt::QueuedConnection);
    connect(checker, &DuplicateChecker::finished, this,
            [this](bool full, bool cancelled, int) {
        if (full) {
            checkingAll = false;
            checkedAllCache = -1;
        }
        emit finished(full, cancelled);
    }, Qt::QueuedConnection);

    /*  THE HOOKS RUN ON WHATEVER THREAD CHANGED THE ROWS, with the catalog unlocked, so
        all they do is hop to this thread. */
    Catalog::instance().setContentChangedHook([this](const QStringList &keys) {
        QMetaObject::invokeMethod(this, [this, keys] { onContentChanged(keys); },
                                  Qt::QueuedConnection);
    });
    Catalog::instance().setMembershipChangedHook([this] {
        QMetaObject::invokeMethod(this, [this] { requestRebuild(); },
                                  Qt::QueuedConnection);
    });

    /* The answer stored by the last session, before anything changes. */
    requestRebuild();
}

QStringList Duplicates::twinsOf(const QString &path) const
{
    const int group = statusByPath.value(path).group;
    if (group <= 0) return {};
    QStringList out = pathsByGroup.value(group);
    out.removeAll(path);
    return out;
}

void Duplicates::shutdown()
{
    Catalog::instance().setContentChangedHook(nullptr);
    Catalog::instance().setMembershipChangedHook(nullptr);
    if (checker) checker->shutdown();
}

void Duplicates::checkAll()
{
    if (!checker || checkingAll) return;
    checkingAll = true;
    const int gen = ++fullGen;
    const QSet<QString> ok = DuplicateOk::Paths();
    QMetaObject::invokeMethod(checker, [c = checker, gen, ok] { c->checkAll(gen, ok); },
                              Qt::QueuedConnection);
}

void Duplicates::stop()
{
    if (checker && checkingAll) checker->stopFull(fullGen);
}

void Duplicates::requestRebuild()
{
    if (!checker || rebuildQueued) return;
    rebuildQueued = true;
    QTimer::singleShot(kRebuildDebounceMs, this, [this] {
        rebuildQueued = false;
        const QSet<QString> ok = DuplicateOk::Paths();
        QMetaObject::invokeMethod(checker, [c = checker, ok] { c->rebuild(ok); },
                                  Qt::QueuedConnection);
    });
}

void Duplicates::onContentChanged(const QStringList &pathkeys)
{
    pendingKeys << pathkeys;
    if (keysQueued) return;
    keysQueued = true;
    QTimer::singleShot(kKeysDebounceMs, this, [this] { flushPendingKeys(); });
}

void Duplicates::flushPendingKeys()
{
    keysQueued = false;
    if (!checker || pendingKeys.isEmpty()) return;
    QStringList keys = std::exchange(pendingKeys, {});
    keys.removeDuplicates();
    const QSet<QString> ok = DuplicateOk::Paths();
    QMetaObject::invokeMethod(checker, [c = checker, keys, ok] { c->checkKeys(keys, ok); },
                              Qt::QueuedConnection);
}

bool Duplicates::hasCheckedAll()
{
    if (checkedAllCache < 0)
        checkedAllCache = Catalog::instance().metaValue(kCheckedKey).isEmpty() ? 0 : 1;
    return checkedAllCache == 1;
}
