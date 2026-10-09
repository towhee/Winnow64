#include "Datamodel/keywordauditstore.h"
#include "Datamodel/userdb.h"
#include "Metadata/keywordpaths.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QVariant>

namespace KeywordAuditStore {

QSet<QString> dismissed()
{
    QSet<QString> out;
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) return out;
    QSqlQuery q(d);
    q.setForwardOnly(true);
    if (!q.exec("SELECT pathkey, kind, keyword FROM audit_verdict WHERE verdict = 'correct'"))
        return out;
    while (q.next())
        out.insert(q.value(0).toString() + '|' + q.value(1).toString() + '|'
                   + q.value(2).toString());
    return out;
}

bool setDismissed(const QString &pathKey, const QString &kind, const QString &keyword,
                  bool on)
{
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) return false;
    QSqlQuery q(d);
    if (on) {
        q.prepare("INSERT INTO audit_verdict (pathkey, kind, keyword, verdict, at)"
                  " VALUES (?, ?, ?, 'correct', ?)"
                  " ON CONFLICT(pathkey, kind, keyword) DO UPDATE SET"
                  " verdict = excluded.verdict, at = excluded.at");
        q.addBindValue(pathKey);
        q.addBindValue(kind);
        q.addBindValue(keywordFold(keyword));
        q.addBindValue(QDateTime::currentSecsSinceEpoch());
    }
    else {
        q.prepare("DELETE FROM audit_verdict WHERE pathkey = ? AND kind = ? AND keyword = ?");
        q.addBindValue(pathKey);
        q.addBindValue(kind);
        q.addBindValue(keywordFold(keyword));
    }
    return q.exec();
}

QHash<QString, QString> skipped()
{
    QHash<QString, QString> out;
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) return out;
    QSqlQuery q(d);
    q.setForwardOnly(true);
    if (!q.exec("SELECT keyword, display FROM audit_skip")) return out;
    while (q.next()) out.insert(q.value(0).toString(), q.value(1).toString());
    return out;
}

bool setSkipped(const QString &keyword, bool on)
{
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) return false;
    QSqlQuery q(d);
    if (on) {
        q.prepare("INSERT INTO audit_skip (keyword, display) VALUES (?, ?)"
                  " ON CONFLICT(keyword) DO UPDATE SET display = excluded.display");
        q.addBindValue(keywordFold(keyword));
        q.addBindValue(keyword);
    }
    else {
        q.prepare("DELETE FROM audit_skip WHERE keyword = ?");
        q.addBindValue(keywordFold(keyword));
    }
    return q.exec();
}

QHash<QString, QString> vocabulary()
{
    QHash<QString, QString> out;
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) return out;
    QSqlQuery q(d);
    q.setForwardOnly(true);
    if (!q.exec("SELECT pathfold, path FROM vocab")) return out;
    while (q.next()) out.insert(q.value(0).toString(), q.value(1).toString());
    return out;
}

int clearDismissed()
{
    QSqlDatabase d = UserDb::instance().db();
    if (!d.isOpen()) return 0;
    QSqlQuery q(d);
    return q.exec("DELETE FROM audit_verdict") ? q.numRowsAffected() : 0;
}

}   // namespace KeywordAuditStore
