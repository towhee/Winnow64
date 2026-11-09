#ifndef DEVPREVIEWREADY_H
#define DEVPREVIEWREADY_H

#include <QHash>
#include <QSet>
#include <QString>

class DataModel;

/*
    DOES THIS ROW HAVE A CURRENT devPreview? -- for the thumbnail cache badge.

    The badge is green when the loupe would show a developed picture without a decode,
    and a built devPreview is that picture whether or not ImageCache holds it: Develop
    mode trims ImageCache to the current image, so every other thumbnail read red while
    its devPreview sat ready on disk.

    DevPreviewCache cannot answer from paint(): it is SQLite only (Cache/devpreviewcache.h
    keeps nothing per image in memory, by design, for 250,000 images) and its answer
    also stats the source file. So this is a MEMO of its answers, filled LAZILY for the
    rows that are painted: an unknown row returns false and is queued, one batched probe
    runs on a pool thread, and a row whose answer turned out true is repainted through
    DataModel::scheduleVisibleEmit.

    STALENESS. An entry remembers the recipe base key (G::DevPreviewKeyColumn) it was
    probed under and the cache's generation (DevPreviewCache::generation, bumped by every
    put, removal, eviction, move and clear). Either moving makes the entry stale: it is
    still returned, so the badge does not flicker while it is re-probed, and the re-probe
    repaints the row if the answer changed. A dm->instance change drops the memo.

    GUI thread only, except the probe body, which touches nothing of this object.
*/
class DevPreviewReady
{
public:
    explicit DevPreviewReady(DataModel *dm) : dm(dm) {}

    /* rowKey is G::KeyRole (a version is its own row), baseKey the row's
       G::DevPreviewKeyColumn. */
    bool isReady(const QString &rowKey, const QString &baseKey);

private:
    struct Entry {
        QString baseKey;
        quint64 gen = 0;
        bool ready = false;
    };
    void queue(const QString &rowKey, const QString &baseKey);
    void flush();

    DataModel *dm;
    int memoInstance = -1;
    QHash<QString, Entry> memo;
    QHash<QString, QString> pending;    // rowKey -> baseKey, waiting for flush()
    QSet<QString> inFlight;             // rowKeys a probe is answering now
    bool flushQueued = false;
};

#endif // DEVPREVIEWREADY_H
