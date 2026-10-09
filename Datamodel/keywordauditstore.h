#ifndef KEYWORDAUDITSTORE_H
#define KEYWORDAUDITSTORE_H

#include <QHash>
#include <QSet>
#include <QString>

/*
    WHAT THE USER DECIDED ABOUT AUDIT KEYWORDS FINDINGS -- userdata.db schema 3
    (Datamodel/userdb.h), so it survives a rebuilt index.

      verdicts  findings judged "the keywords are right". The next audit leaves them out
                (VisualAudit::run's dismissed set), so a reviewed catalog does not
                re-present the same false alarms every time.
      skipped   keywords excluded from the audit altogether -- typically ones a picture
                cannot show (a client, a project) that still scored as "visual" because
                they happen to cluster by shoot.

    A finding the user judged a MISTAKE is not remembered: fixing it changes the
    keywords, and the finding is gone on its own.

    GUI THREAD, like every user of UserDb. A closed userdata.db answers empty and
    writes nothing.
*/
namespace KeywordAuditStore {

/* VisualAudit::verdictKey strings ("pathKey|kind|foldedKeyword") of dismissed
   findings. */
QSet<QString> dismissed();

/* Remember (or forget) that one image's finding was judged correct. keyword is the
   display path; it is folded here. kind is "suspect" or "missing". */
bool setDismissed(const QString &pathKey, const QString &kind, const QString &keyword,
                  bool on);

/* Folded keyword path -> display path, for every keyword excluded from the audit. */
QHash<QString, QString> skipped();
bool setSkipped(const QString &keyword, bool on);

/* Folded path -> display path of every keyword in the user's list (userdata.db vocab),
   read directly so an audit does not depend on the Keywords dock having loaded. */
QHash<QString, QString> vocabulary();

/* Forget every verdict -- "Show dismissed findings again". Returns rows removed. */
int clearDismissed();

}   // namespace KeywordAuditStore

#endif // KEYWORDAUDITSTORE_H
