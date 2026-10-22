#include "Main/keywordsuggester.h"
#include "Cache/catalog.h"
#include "Cache/embeddingstore.h"
#include "Cache/pathkey.h"
#include "Cache/thumbcache.h"
#include "Main/global.h"
#include "Main/keywordauditjob.h"
#include "Metadata/keywordpaths.h"
#include "Utilities/inference/imageembedder.h"

#include <QDeadlineTimer>
#include <QLocale>

KeywordSuggester::KeywordSuggester(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<VisualAudit::Suggestion>("VisualAudit::Suggestion");
    qRegisterMetaType<QVector<VisualAudit::Suggestion>>("QVector<VisualAudit::Suggestion>");
    moveToThread(&thread);
    thread.start(QThread::LowPriority);
}

KeywordSuggester::~KeywordSuggester()
{
    shutdown();
}

void KeywordSuggester::shutdown(int maxWaitMs)
{
    ++latest;                               // anything still queued is now stale
    if (!thread.isRunning()) return;
    thread.quit();
    thread.wait(QDeadlineTimer(maxWaitMs));
}

void KeywordSuggester::setVocab(const QHash<QString, QString> &v)
{
    vocab = v;
}

void KeywordSuggester::reloadModel()
{
    modelLoaded = true;
    if (!model.load(KeywordAuditJob::suggestModelPath())) model = {};
    /*  Vectors from another network are not comparable; a model learned before an
        embedder change suggests nothing visual until the next audit. */
    if (model.isValid() && model.modelTag != ImageEmbedder::ModelTag()) model = {};
}

void KeywordSuggester::request(int generation, const QString &path,
                               const QStringList &carried,
                               const QSet<QString> &dismissedFold)
{
    if (generation != latest.load()) return;           // overtaken
    if (!modelLoaded) reloadModel();

    const VisualAudit::Params params;
    QString status;
    QVector<VisualAudit::Suggestion> visual, session;

    const CatalogRow row = Catalog::instance().rowsForPaths({path}).value(path);
    if (row.path.isEmpty()) {
        emit ready(generation, path, {},
                   tr("Suggestions need the image in the catalog."));
        return;
    }

    QSet<QString> carriedFold;
    for (const QString &p : keywordPrefixExpand(carried)) carriedFold.insert(keywordFold(p));

    /* ---------------------------------------------------------------- visual */
    if (model.isValid()) {
        const QString key = cachePathKey(path);
        QVector<float> vec = EmbeddingStore::get(model.modelTag, key, model.dim,
                                                 row.srcSize, row.srcMtime);
        if (vec.isEmpty() && ImageEmbedder::IsModelPresent()
            && !ImageEmbedder::NeedsFirstCompile()) {
            const QImage img = ThumbCache::instance().getImage(path, false, row.srcSize,
                                                               row.srcMtime);
            QVector<QVector<float>> out;
            if (!img.isNull() && ImageEmbedder::Embed({img}, out) && !out.isEmpty()
                && out.first().size() == model.dim) {
                vec = out.first();
                EmbeddingStore::put(model.modelTag,
                                    {{key, row.srcSize, row.srcMtime, vec}});
            }
        }
        if (generation != latest.load()) return;
        if (!vec.isEmpty()) visual = model.score(vec.constData(), carriedFold, dismissedFold);
        status = tr("Learned %1 from %2 images by Audit keywords.")
                     .arg(QLocale().toString(model.learned.date(), QLocale::ShortFormat),
                          QLocale().toString(model.images));
    }
    else {
        status = tr("Run Audit keywords to add suggestions from how images look.");
    }

    /* ---------------------------------------------------------------- session */
    if (row.captured.isValid() && !row.folder.isEmpty()) {
        CatalogQuery q;
        q.folder = row.folder;
        q.from = row.captured.addSecs(-params.sessionSecs);
        q.to = row.captured.addSecs(params.sessionSecs);
        QVector<VisualAudit::SessionImage> nearby;
        for (const CatalogRow &r : Catalog::instance().searchRows(q, 0)) {
            if (r.path == path || !r.captured.isValid()) continue;
            const QStringList eff = keywordEffectivePaths(r.keywordsLiteral, r.keywordPaths);
            if (eff.isEmpty()) continue;
            nearby << VisualAudit::SessionImage{r.folder, Catalog::wallClockSecs(r.captured),
                                                keywordPruneAncestors(eff)};
        }
        session = VisualAudit::sessionSuggestions(
            {row.folder, Catalog::wallClockSecs(row.captured), {}}, nearby, vocab,
            carriedFold, dismissedFold, params);
    }

    if (generation != latest.load()) return;
    emit ready(generation, path, VisualAudit::mergeSuggestions(visual, session, params),
               status);
}
