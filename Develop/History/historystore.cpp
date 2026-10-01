#include "Develop/History/historystore.h"
#include "Cache/cachedb.h"
#include "Cache/pathkey.h"
#include <QAtomicInt>
#include <QDateTime>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QVariant>

namespace HistoryStore {

namespace {

/* Prune once per this many saves rather than on every one: it is a sort over the whole
   table, and the bound is generous enough to overshoot by a few. */
constexpr int kPruneEvery = 64;
QAtomicInt saveCount;

void prune(QSqlDatabase &db)
{
    QSqlQuery q(db);
    q.prepare("DELETE FROM develop_history WHERE pathkey NOT IN"
              " (SELECT pathkey FROM develop_history ORDER BY saved DESC LIMIT ?)");
    q.addBindValue(kMaxImages);
    q.exec();
}

}   // namespace

bool load(const QString &path, QVector<HistoryEntry> &entries, int &pos,
          QString &recipeStamp)
{
    entries.clear();
    pos = -1;
    recipeStamp.clear();
    const QString key = cachePathKey(path);
    if (key.isEmpty()) return false;
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("SELECT pos, recipe, entries FROM develop_history WHERE pathkey = ?");
    q.addBindValue(key);
    if (!q.exec() || !q.next()) return false;
    pos = q.value(0).toInt();
    recipeStamp = q.value(1).toString();
    const QByteArray json = qUncompress(q.value(2).toByteArray());
    const QJsonArray a = QJsonDocument::fromJson(json).array();
    entries.reserve(a.size());
    for (const QJsonValue &v : a) entries.append(HistoryEntry::fromJson(v.toObject()));
    return !entries.isEmpty() && pos >= 0 && pos < entries.size();
}

void save(const QString &path, const QVector<HistoryEntry> &entries, int pos,
          const QString &recipeStamp)
{
    if (entries.isEmpty()) { remove(path); return; }
    const QString key = cachePathKey(path);
    if (key.isEmpty()) return;
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return;

    QJsonArray a;
    for (const HistoryEntry &e : entries) a.append(e.toJson());
    const QByteArray blob = qCompress(QJsonDocument(a).toJson(QJsonDocument::Compact), 6);

    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO develop_history"
              " (pathkey, path, pos, recipe, entries, saved) VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(key);
    q.addBindValue(path);
    q.addBindValue(pos);
    q.addBindValue(recipeStamp);
    q.addBindValue(blob);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.exec();

    if (saveCount.fetchAndAddRelaxed(1) % kPruneEvery == kPruneEvery - 1) prune(db);
}

void remove(const QString &path)
{
    const QString key = cachePathKey(path);
    if (key.isEmpty()) return;
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("DELETE FROM develop_history WHERE pathkey = ?");
    q.addBindValue(key);
    q.exec();
}

void onMoved(const QString &srcPath, const QString &dstPath)
{
    const QString from = cachePathKey(srcPath);
    const QString to = cachePathKey(dstPath);
    if (from.isEmpty() || to.isEmpty()) return;
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return;

    /* The image and its versions ("<path>/#v<id>"). Whatever the destination held is
       replaced: the history that travels with the file is the one that describes it.
       Selected rather than updated in place so each version's suffix can be carried. */
    QSqlQuery sel(db);
    sel.prepare("SELECT pathkey, path FROM develop_history"
                " WHERE pathkey = ? OR substr(pathkey, 1, ?) = ?");
    const QString vPrefix = from + "/#v";
    sel.addBindValue(from);
    sel.addBindValue(vPrefix.size());
    sel.addBindValue(vPrefix);
    if (!sel.exec()) return;
    QVector<QPair<QString, QString>> rows;
    while (sel.next()) rows.append({sel.value(0).toString(), sel.value(1).toString()});
    if (rows.isEmpty()) return;

    db.transaction();
    for (const auto &r : rows) {
        const QString suffix = r.first.mid(from.size());       // "" or "/#v<id>"
        QSqlQuery d(db);
        d.prepare("DELETE FROM develop_history WHERE pathkey = ?");
        d.addBindValue(to + suffix);
        d.exec();
        QSqlQuery u(db);
        u.prepare("UPDATE develop_history SET pathkey = ?, path = ? WHERE pathkey = ?");
        u.addBindValue(to + suffix);
        u.addBindValue(dstPath + suffix);
        u.addBindValue(r.first);
        u.exec();
    }
    db.commit();
}

void onDeleted(const QString &path)
{
    const QString key = cachePathKey(path);
    if (key.isEmpty()) return;
    QSqlDatabase db = CacheDb::instance().db();
    if (!db.isOpen()) return;
    const QString vPrefix = key + "/#v";
    QSqlQuery q(db);
    q.prepare("DELETE FROM develop_history WHERE pathkey = ? OR substr(pathkey, 1, ?) = ?");
    q.addBindValue(key);
    q.addBindValue(vPrefix.size());
    q.addBindValue(vPrefix);
    q.exec();
}

}   // namespace HistoryStore
