#include "Cache/embeddingstore.h"
#include "Cache/cachedb.h"

#include <QFloat16>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QtEndian>

namespace EmbeddingStore {

QByteArray encode(const QVector<float> &vec)
{
    QByteArray blob(vec.size() * int(sizeof(qfloat16)), Qt::Uninitialized);
    auto *out = reinterpret_cast<qfloat16 *>(blob.data());
    qFloatToFloat16(out, vec.constData(), vec.size());
    /* qfloat16 is host order; every platform Winnow builds for is little-endian, and
       this keeps the file portable if that ever changes. */
    if constexpr (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        auto *u = reinterpret_cast<quint16 *>(blob.data());
        for (qsizetype i = 0; i < vec.size(); ++i) u[i] = qToLittleEndian(u[i]);
    }
    return blob;
}

QVector<float> decode(const QByteArray &blob, int dim)
{
    if (dim <= 0 || blob.size() != dim * int(sizeof(qfloat16))) return {};
    QVector<float> vec(dim);
    if constexpr (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        QByteArray copy = blob;
        auto *u = reinterpret_cast<quint16 *>(copy.data());
        for (int i = 0; i < dim; ++i) u[i] = qFromLittleEndian(u[i]);
        qFloatFromFloat16(vec.data(), reinterpret_cast<const qfloat16 *>(copy.constData()),
                          dim);
    }
    else {
        qFloatFromFloat16(vec.data(), reinterpret_cast<const qfloat16 *>(blob.constData()),
                          dim);
    }
    return vec;
}

QHash<QString, Stamp> stamps(const QString &model)
{
    QHash<QString, Stamp> out;
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return out;
    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare("SELECT pathkey, srcsize, srcmtime FROM image_embedding WHERE model = ?");
    q.addBindValue(model);
    if (!q.exec()) return out;
    while (q.next())
        out.insert(q.value(0).toString(), {q.value(1).toLongLong(), q.value(2).toLongLong()});
    return out;
}

int put(const QString &model, const QVector<Entry> &entries)
{
    if (entries.isEmpty()) return 0;
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen() || !db.transaction()) return 0;
    QSqlQuery q(db);
    q.prepare("INSERT INTO image_embedding (pathkey, model, srcsize, srcmtime, vec)"
              " VALUES (?, ?, ?, ?, ?)"
              " ON CONFLICT(pathkey) DO UPDATE SET model = excluded.model,"
              " srcsize = excluded.srcsize, srcmtime = excluded.srcmtime,"
              " vec = excluded.vec");
    int n = 0;
    for (const Entry &e : entries) {
        if (e.pathKey.isEmpty() || e.vec.isEmpty()) continue;
        q.addBindValue(e.pathKey);
        q.addBindValue(model);
        q.addBindValue(e.srcSize);
        q.addBindValue(e.srcMtime);
        q.addBindValue(encode(e.vec));
        if (q.exec()) ++n;
    }
    if (!db.commit()) {
        db.rollback();
        return 0;
    }
    return n;
}

bool loadAll(const QString &model, int dim, QStringList &keys, std::vector<float> &mat)
{
    keys.clear();
    mat.clear();
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare("SELECT e.pathkey, e.vec FROM image_embedding e"
              " JOIN image i ON i.pathkey = e.pathkey"
              " WHERE e.model = ? AND i.live = 1"
              " AND e.srcsize = i.srcsize AND e.srcmtime = i.srcmtime");
    q.addBindValue(model);
    if (!q.exec()) return false;
    while (q.next()) {
        const QVector<float> v = decode(q.value(1).toByteArray(), dim);
        if (v.isEmpty()) continue;
        keys << q.value(0).toString();
        mat.insert(mat.end(), v.cbegin(), v.cend());
    }
    return true;
}

int prune(const QString &model)
{
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    q.prepare("DELETE FROM image_embedding WHERE model <> ?"
              " OR pathkey NOT IN (SELECT pathkey FROM image)");
    q.addBindValue(model);
    return q.exec() ? q.numRowsAffected() : 0;
}

int count(const QString &model)
{
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM image_embedding WHERE model = ?");
    q.addBindValue(model);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

}   // namespace EmbeddingStore
