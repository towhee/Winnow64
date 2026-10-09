#ifndef EMBEDDINGSTORE_H
#define EMBEDDINGSTORE_H

#include <QHash>
#include <QString>
#include <QVector>
#include <vector>

/*
    IMAGE EMBEDDINGS IN THE LOCAL INDEX -- the image_embedding table (index.db schema
    22), one SigLIP vector per image, for Audit Keywords (Utilities/keywordaudit.h).

    KEYED ON pathkey, like thumb and devpreview, so a row matches the catalog's image
    row without a join table. STAMPED with the source file's size and mtime -- the same
    pair the thumb table carries, from the catalog row -- so a file edited outside
    Winnow is re-embedded rather than compared by a picture it no longer is. TAGGED with
    the model that made it, because vectors from two networks are not comparable: a
    model change is a miss, never a mix.

    STORED AS float16, little-endian, which halves a 768-d vector to 1.5 KB with no
    measurable effect on the audit (the Phase 0 prototype ran on float16 vectors).

    DERIVED DATA, like everything in index.db: losing it costs a re-embed, never a
    keyword. The user's verdicts on findings are user data and live in userdata.db.

    THREADING: every function uses the calling thread's CacheDb connection, so any
    thread may call them. A closed database answers "nothing stored" and writes nothing.
*/
namespace EmbeddingStore {

struct Entry
{
    QString pathKey;            // cachePathKey(path)
    qint64 srcSize = 0;
    qint64 srcMtime = 0;
    QVector<float> vec;         // L2-normalised
};

struct Stamp
{
    qint64 srcSize = 0;
    qint64 srcMtime = 0;
};

/* float32 <-> the stored float16 BLOB. decode returns empty when the blob is not
   exactly dim halves. */
QByteArray encode(const QVector<float> &vec);
QVector<float> decode(const QByteArray &blob, int dim);

/* The stamps of every row this model produced, by pathkey -- what the embed job
   compares with the catalog to find the images still to do. */
QHash<QString, Stamp> stamps(const QString &model);

/* Insert or replace, in one transaction. Returns the number written. */
int put(const QString &model, const QVector<Entry> &entries);

/*  Every vector this model produced for a LIVE catalog image whose stamps still match
    the catalog row, as a row-major keys.size() x dim matrix. Rows for images edited
    since they were embedded are left out: comparing a stale picture would report a
    "mistake" in an image that no longer looks like that. */
bool loadAll(const QString &model, int dim, QStringList &keys, std::vector<float> &mat);

/* Delete rows whose image is no longer in the catalog, or that another model made.
   Returns the number deleted. */
int prune(const QString &model);

/* Rows stored for this model. */
int count(const QString &model);

}   // namespace EmbeddingStore

#endif // EMBEDDINGSTORE_H
