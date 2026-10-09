#include "Main/keywordauditjob.h"
#include "Cache/catalog.h"
#include "Cache/embeddingstore.h"
#include "Cache/pathkey.h"
#include "Cache/thumbcache.h"
#include "Main/global.h"
#include "Metadata/keywordpaths.h"
#include "Metadata/metadata.h"
#include "Utilities/inference/imageembedder.h"

#include <QBuffer>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QTransform>

namespace {

constexpr int kEmbedBatch = 64;         // images per Embed() call and per DB commit
constexpr int kReportMs = 250;
constexpr int kPauseSliceMs = 200;

/* The vectors this build compares, tagged so another model's rows are misses. */
QString modelTag() { return ImageEmbedder::ModelTag(); }

QElapsedTimer &reportClock()
{
    static QElapsedTimer t;
    if (!t.isValid()) t.start();
    return t;
}

}   // namespace

KeywordAuditJob::KeywordAuditJob(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<KeywordAuditOutcome>("KeywordAuditOutcome");
    moveToThread(&jobThread);
    jobThread.start(QThread::LowPriority);
}

KeywordAuditJob::~KeywordAuditJob()
{
    shutdown();
    delete metadata;
}

void KeywordAuditJob::shutdown(int maxWaitMs)
{
    stop();
    if (!jobThread.isRunning()) return;
    jobThread.quit();
    jobThread.wait(QDeadlineTimer(maxWaitMs));
}

void KeywordAuditJob::stop()
{
    abort.store(true, std::memory_order_relaxed);
}

bool KeywordAuditJob::shouldPause() const
{
    /* The same courtesy CatalogScanner shows: a folder load is what the user is waiting
       on. G::stop is a transient teardown latch, so it pauses rather than aborts. */
    return G::isModifyingDatamodel || G::isLoadRunning || G::stop;
}

bool KeywordAuditJob::waitWhilePaused()
{
    if (!shouldPause()) return !abort.load(std::memory_order_relaxed);
    report(tr("Paused while a folder loads"), 0, 0, true);
    while (shouldPause()) {
        if (abort.load(std::memory_order_relaxed)) return false;
        QThread::msleep(kPauseSliceMs);
    }
    return !abort.load(std::memory_order_relaxed);
}

void KeywordAuditJob::report(const QString &stage, int done, int total, bool force)
{
    const qint64 now = reportClock().elapsed();
    if (!force && lastReportMs >= 0 && now - lastReportMs < kReportMs) return;
    lastReportMs = now;
    emit progress(stage, done, total);
}

QImage KeywordAuditJob::loadPicture(const QString &path, qint64 srcSize, qint64 srcMtime)
{
/*
    The picture the model sees. The cached browsing thumbnail first -- ~256 px, already
    rotated, one indexed read. hasDevelopRecipe is false on purpose: the audit asks what
    was PHOTOGRAPHED, so the camera's original is the right picture even for an edited
    image.

    On a miss (an image catalogued by a scan but never browsed) the file's own embedded
    preview, found the way Thumb::loadFromJpgData finds it, decoded small and rotated
    by its EXIF orientation as the cached thumbnails are. A format with no embedded
    preview is decoded whole, scaled by the reader.
*/
    QImage img = ThumbCache::instance().getImage(path, false, srcSize, srcMtime);
    if (!img.isNull()) return img;

    if (!metadata) metadata = new Metadata;
    const QFileInfo fi(path);
    int orientation = 0;
    quint32 offset = 0, length = 0;
    if (metadata->loadImageMetadata(fi, 0, G::dmInstance, true, false, false, false,
                                    "KeywordAuditJob::loadPicture")) {
        const ImageMetadata &m = metadata->m;
        orientation = m.orientation;
        if (m.lengthThumb > 0) { offset = m.offsetThumb; length = m.lengthThumb; }
        else if (m.lengthFull > 0) { offset = m.offsetFull; length = m.lengthFull; }
    }

    const QSize target(512, 512);
    if (length > 0) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly) && f.seek(offset)) {
            QByteArray bytes = f.read(length);
            QBuffer buf(&bytes);
            QImageReader r(&buf);
            const QSize s = r.size();
            if (s.isValid() && (s.width() > target.width() || s.height() > target.height()))
                r.setScaledSize(s.scaled(target, Qt::KeepAspectRatio));
            img = r.read();
        }
    }
    if (img.isNull()) {
        QImageReader r(path);
        r.setAutoTransform(true);              // orientation applied by the reader
        const QSize s = r.size();
        if (s.isValid()) r.setScaledSize(s.scaled(target, Qt::KeepAspectRatio));
        return r.read();
    }

    int degrees = 0;
    switch (orientation) {
        case 3: degrees = 180; break;
        case 6: degrees = 90; break;
        case 8: degrees = 270; break;
    }
    if (degrees) img = img.transformed(QTransform().rotate(degrees));
    return img;
}

void KeywordAuditJob::run(const QHash<QString, QString> &vocab,
                          const QSet<QString> &dismissed, const QSet<QString> &skipped)
{
    if (running.exchange(true)) return;
    abort.store(false, std::memory_order_relaxed);
    QElapsedTimer t;
    t.start();
    KeywordAuditOutcome out;

    auto finish = [&](const QString &error = {}) {
        out.error = error;
        out.cancelled = abort.load(std::memory_order_relaxed);
        out.ms = t.elapsed();
        running.store(false);
        emit finished(out);
    };

    /* ---------------------------------------------------------------- catalog */
    report(tr("Reading the catalog"), 0, 0, true);
    QVector<CatalogRow> rows = Catalog::instance().searchRows(CatalogQuery{}, 0);
    rows.erase(std::remove_if(rows.begin(), rows.end(), [](const CatalogRow &r) {
                   return r.keywordsLiteral.isEmpty() && r.keywordPaths.isEmpty();
               }), rows.end());
    if (rows.isEmpty()) {
        finish(tr("No catalogued image has keywords yet."));
        return;
    }

    /* ---------------------------------------------------------------- embed */
    const QString model = modelTag();
    EmbeddingStore::prune(model);
    const QHash<QString, EmbeddingStore::Stamp> have = EmbeddingStore::stamps(model);
    QVector<int> todo;
    for (int i = 0; i < rows.size(); ++i) {
        const auto it = have.constFind(cachePathKey(rows[i].path));
        if (it == have.cend() || it->srcSize != rows[i].srcSize
            || it->srcMtime != rows[i].srcMtime)
            todo << i;
    }

    if (!todo.isEmpty()) {
        if (ImageEmbedder::NeedsFirstCompile())
            report(tr("Preparing the image model for this computer "
                      "(once, about a minute and a half)"), 0, 0, true);
        for (int s = 0; s < todo.size(); s += kEmbedBatch) {
            if (!waitWhilePaused()) { finish(); return; }
            const int e = std::min<int>(todo.size(), s + kEmbedBatch);
            QVector<QImage> pics;
            for (int k = s; k < e; ++k) {
                const CatalogRow &r = rows[todo[k]];
                pics << loadPicture(r.path, r.srcSize, r.srcMtime);
            }
            QVector<QVector<float>> vecs;
            if (!ImageEmbedder::Embed(pics, vecs)) {
                finish(tr("The image model could not be run (%1).")
                           .arg(ImageEmbedder::BackendName()));
                return;
            }
            QVector<EmbeddingStore::Entry> entries;
            for (int k = s; k < e; ++k) {
                const CatalogRow &r = rows[todo[k]];
                const QVector<float> &v = vecs[k - s];
                if (v.isEmpty()) { ++out.unreadable; continue; }
                entries.push_back({cachePathKey(r.path), r.srcSize, r.srcMtime, v});
            }
            out.embedded += EmbeddingStore::put(model, entries);
            report(tr("Reading images"), e, todo.size());
        }
    }

    /* ---------------------------------------------------------------- score */
    report(tr("Loading image vectors"), 0, 0, true);
    QStringList keys;
    std::vector<float> mat;
    if (!EmbeddingStore::loadAll(model, ImageEmbedder::kDim, keys, mat)) {
        finish(tr("The image vectors could not be read from the index."));
        return;
    }
    QHash<QString, int> rowOf;
    for (int i = 0; i < rows.size(); ++i) rowOf.insert(cachePathKey(rows[i].path), i);

    QVector<VisualAudit::Image> images;
    std::vector<float> emb;
    emb.reserve(mat.size());
    for (int k = 0; k < keys.size(); ++k) {
        const int i = rowOf.value(keys[k], -1);
        if (i < 0) continue;                         // no longer keyworded
        const CatalogRow &r = rows[i];
        const QStringList eff = keywordEffectivePaths(r.keywordsLiteral, r.keywordPaths);
        VisualAudit::Image im;
        im.pathKey = keys[k];
        im.folder = r.folder;
        im.captured = r.captured.isValid() ? r.captured.toSecsSinceEpoch() : 0;
        im.explicitPaths = keywordPruneAncestors(eff);
        im.expandedPaths = keywordPrefixExpand(eff);
        images << im;
        out.images.push_back({r.path, keys[k], im.explicitPaths, r.keywordsLiteral,
                              r.keywordPaths});
        const auto *v = mat.data() + size_t(k) * ImageEmbedder::kDim;
        emb.insert(emb.end(), v, v + ImageEmbedder::kDim);
    }
    mat.clear();
    mat.shrink_to_fit();

    out.result = VisualAudit::run(images, emb, ImageEmbedder::kDim, vocab, dismissed,
                                   skipped, {}, [&](const QString &stage, int pct) {
        report(stage, pct, 0);
        return !abort.load(std::memory_order_relaxed);
    });
    out.ok = out.result.ok;
    finish();
}
