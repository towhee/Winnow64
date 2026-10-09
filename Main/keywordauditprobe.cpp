#include "Main/keywordauditprobe.h"
#include "Cache/catalog.h"
#include "Datamodel/keywordauditstore.h"
#include "Main/keywordauditjob.h"
#include "Utilities/inference/imageembedder.h"

#include <QMap>
#include <cstdio>

namespace {

void say(const QString &s)
{
    fprintf(stderr, "%s\n", qPrintable(s));
    fflush(stderr);
}

}   // namespace

namespace KeywordAuditProbe {

int run(const QString &keywordFilter)
{
    say("");
    say("KEYWORD AUDIT PROBE");
    if (!ImageEmbedder::IsSupportedBuild()) {
        say("  UNAVAILABLE -- this build has no inference runtime (WINNOW_ENABLE_ORT).");
        return 2;
    }
    if (!ImageEmbedder::IsModelPresent()) {
        say("  UNAVAILABLE -- siglip2_image.onnx is not installed (Help > Manage AI "
            "models, or beside the binary).");
        return 2;
    }
    if (!Catalog::instance().isAvailable()) {
        say("  UNAVAILABLE -- the catalog could not be opened.");
        return 2;
    }

    const QHash<QString, QString> vocab = KeywordAuditStore::vocabulary();
    const QSet<QString> dismissed = KeywordAuditStore::dismissed();
    const QList<QString> skipKeys = KeywordAuditStore::skipped().keys();
    const QSet<QString> skipped(skipKeys.begin(), skipKeys.end());
    say(QString("  vocabulary %1 keywords, %2 dismissed findings, %3 excluded keywords")
            .arg(vocab.size()).arg(dismissed.size()).arg(skipped.size()));
    if (ImageEmbedder::NeedsFirstCompile())
        say("  first run on this machine: the model is compiled once (~90 s)");

    /*  The job's own run(), called on this thread: its signals reach these lambdas
        directly, so nothing needs an event loop. */
    KeywordAuditJob job;
    QString lastStage;
    QObject::connect(&job, &KeywordAuditJob::progress, &job,
                     [&](const QString &stage, int done, int total) {
        if (stage != lastStage || total > 0)
            say(total > 0 ? QString("  %1 %2/%3").arg(stage).arg(done).arg(total)
                          : QString("  %1 %2").arg(stage).arg(done ? QString::number(done)
                                                                       + "%" : QString()));
        lastStage = stage;
    }, Qt::DirectConnection);
    KeywordAuditOutcome out;
    QObject::connect(&job, &KeywordAuditJob::finished, &job,
                     [&](const KeywordAuditOutcome &o) { out = o; }, Qt::DirectConnection);
    job.run(vocab, dismissed, skipped);
    job.shutdown();

    if (!out.ok) {
        say("  FAILED -- " + (out.error.isEmpty() ? QString("no result") : out.error));
        return 2;
    }

    const auto &res = out.result;
    int sus = 0, visual = 0;
    for (const auto &f : res.findings) sus += f.kind == VisualAudit::Finding::Suspect;
    for (const auto &s : res.stats) visual += s.visual;
    say("");
    say(QString("  backend    %1").arg(ImageEmbedder::BackendName()));
    say(QString("  images     %1 audited, %2 embedded this run, %3 unreadable")
            .arg(out.images.size()).arg(out.embedded).arg(out.unreadable));
    say(QString("  bursts     %1 units").arg(res.bursts));
    say(QString("  keywords   %1 learned, %2 visual").arg(res.stats.size()).arg(visual));
    say(QString("  findings   %1 suspect, %2 missing").arg(sus)
            .arg(res.findings.size() - sus));
    say(QString("  time       %1 s").arg(out.ms / 1000.0, 0, 'f', 1));

    /* Per top-level branch: where the findings are. */
    QMap<QString, QPair<int, int>> byRoot;
    for (const auto &f : res.findings) {
        auto &c = byRoot[f.keyword.section('|', 0, 0)];
        (f.kind == VisualAudit::Finding::Suspect ? c.first : c.second)++;
    }
    say("");
    say("  BY BRANCH           suspect  missing");
    for (auto it = byRoot.cbegin(); it != byRoot.cend(); ++it)
        say(QString("  %1 %2 %3").arg(it.key().left(18), -18)
                .arg(it.value().first, 8).arg(it.value().second, 8));

    say("");
    say("  TOP FINDINGS" + (keywordFilter.isEmpty() ? QString()
                                                   : " matching \"" + keywordFilter + "\""));
    int shown = 0;
    for (const auto &f : res.findings) {
        if (!keywordFilter.isEmpty() && !f.keyword.contains(keywordFilter, Qt::CaseInsensitive)
            && !f.alternative.contains(keywordFilter, Qt::CaseInsensitive))
            continue;
        const bool s = f.kind == VisualAudit::Finding::Suspect;
        QString line = QString("  %1 %2  %3").arg(f.score, 0, 'f', 2)
                           .arg(s ? "SUSPECT" : "MISSING").arg(f.keyword);
        if (!f.alternative.isEmpty()) line += "  ->  " + f.alternative;
        if (f.members.size() > 1) line += QString("  (burst of %1)").arg(f.members.size());
        say(line);
        say("        " + out.images[f.members.first()].path);
        if (++shown == 40) break;
    }
    say("");
    return 0;
}

}   // namespace KeywordAuditProbe
