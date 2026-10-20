#ifndef KEYWORDSUGGESTER_H
#define KEYWORDSUGGESTER_H

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVector>
#include <atomic>
#include "Utilities/visualaudit.h"

Q_DECLARE_METATYPE(VisualAudit::Suggestion)

/*
    SUGGESTED KEYWORDS FOR THE CURRENT IMAGE -- the Keywords panel's "Suggested" row.

    TWO SIGNALS, both from VisualAudit (see the header there for the rules):
        visual   the model the last Audit Keywords run learned and saved
                 (KeywordAuditJob::suggestModelPath), scored on the image's SigLIP
                 vector -- read from the index, or embedded here from its cached
                 thumbnail when it has none yet.
        session  the keywords most of the image's session carries: same folder, close
                 capture times, read from the catalog. Needs no audit at all.

    OFF THE GUI THREAD. A miss means an inference run and a catalog query, and the panel
    asks on every change of the current image -- so requests are numbered, and one that
    has been overtaken by a newer request before its turn is dropped unanswered.

    NEVER COMPILES THE MODEL. The first SigLIP run on a Mac compiles it for ~90 s; the
    audit owns that wait and says so. Until it has happened, an image with no stored
    vector gets session suggestions only.
*/
class KeywordSuggester : public QObject
{
    Q_OBJECT

public:
    explicit KeywordSuggester(QObject *parent = nullptr);
    ~KeywordSuggester() override;

    /* Stop and wait (bounded) for the thread. GUI thread. */
    void shutdown(int maxWaitMs = 3000);

    /* GUI thread: number the next request; older ones still queued are dropped. */
    int nextGeneration() { return ++latest; }

    QThread thread;

public slots:
    /* Folded path -> display path of the keyword list; session suggestions are drawn
       from it only. */
    void setVocab(const QHash<QString, QString> &vocab);
    /* Re-read the saved model (a new audit has finished). */
    void reloadModel();
    /*  Suggestions for path. carried: the keyword paths the image carries now (from the
        model, which may be newer than the catalog); dismissedFold: folded keywords the
        user said no to for this image. Answers with ready(). */
    void request(int generation, const QString &path, const QStringList &carried,
                 const QSet<QString> &dismissedFold);

signals:
    /*  status: one line about where suggestions come from, or why there are none --
        shown under them. */
    void ready(int generation, const QString &path,
               const QVector<VisualAudit::Suggestion> &suggestions, const QString &status);

private:
    std::atomic<int> latest{0};
    VisualAudit::SuggestModel model;
    bool modelLoaded = false;
    QHash<QString, QString> vocab;
};

#endif // KEYWORDSUGGESTER_H
