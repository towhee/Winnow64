#ifndef KEYWORDAUDITJOB_H
#define KEYWORDAUDITJOB_H

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

class Metadata;

/* One audited image, as the review dialog needs it: where it is, and what it carries. */
struct KeywordAuditImage
{
    QString path;
    QString pathKey;
    QStringList explicitPaths;      // carried keywords, ancestors dropped
    QStringList keywordsLiteral;    // dc:subject as the file spelled it
    QStringList keywordPaths;       // lr:hierarchicalSubject as the file spelled it
};

struct KeywordAuditOutcome
{
    bool ok = false;
    bool cancelled = false;
    QString error;                  // why ok is false, for the dialog
    VisualAudit::Result result;     // finding members index images below
    QVector<KeywordAuditImage> images;
    int embedded = 0;               // vectors computed this run
    int unreadable = 0;             // images no picture could be read for
    qint64 ms = 0;
};
Q_DECLARE_METATYPE(KeywordAuditOutcome)

/*
    AUDIT KEYWORDS, OFF THE GUI THREAD: bring the image vectors up to date, then score.

    TWO STAGES. First every live, keyworded catalog image whose vector is missing or
    stale (EmbeddingStore stamps vs the catalog row) is embedded -- from its cached
    browsing thumbnail when there is one, otherwise from the file's embedded preview.
    That is the whole catalog the first time (~8 minutes for 100k images on an M1 Ultra
    GPU, plus a one-time ~90 s model compile on a Mac) and only what changed afterwards.
    Then VisualAudit::run scores every vector against the user's keyword list (~25 s at
    87k images).

    ON DEMAND, NOT IN THE BACKGROUND. Nothing runs until the user asks for an audit, so
    the GPU and the disk are never busy for a feature nobody opened.

    IT YIELDS TO THE USER, like CatalogScanner: embedding pauses while a folder loads,
    and every stage checks stop() between batches.

    OWNS ITS OWN Metadata, created on its thread -- needed only for an image with no
    cached thumbnail, to find the embedded preview.
*/
class KeywordAuditJob : public QObject
{
    Q_OBJECT

public:
    explicit KeywordAuditJob(QObject *parent = nullptr);
    ~KeywordAuditJob() override;

    bool isRunning() const { return running.load(std::memory_order_relaxed); }

    /* Stop and wait (bounded) for the thread, before the process can exit. GUI thread. */
    void shutdown(int maxWaitMs = 5000);

    /* The object lives on this thread, so run() never blocks the GUI. */
    QThread jobThread;

public slots:
    /*  vocab: folded path -> display path of every keyword in the user's list.
        dismissed / skipped: see VisualAudit::run. */
    void run(const QHash<QString, QString> &vocab, const QSet<QString> &dismissed,
             const QSet<QString> &skipped);

    /* Any thread. The job ends at its next check. */
    void stop();

signals:
    /* stage is shown as is; total 0 means "percent in done". Throttled. */
    void progress(const QString &stage, int done, int total);
    void finished(const KeywordAuditOutcome &outcome);

private:
    bool shouldPause() const;
    bool waitWhilePaused();
    QImage loadPicture(const QString &path, qint64 srcSize, qint64 srcMtime);
    void report(const QString &stage, int done, int total, bool force = false);

    std::atomic<bool> running{false};
    std::atomic<bool> abort{false};
    Metadata *metadata = nullptr;      // created lazily, on the job thread
    qint64 lastReportMs = -1;
};

#endif // KEYWORDAUDITJOB_H
