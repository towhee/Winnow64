#ifndef USERDB_H
#define USERDB_H

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QString>

/*
    THE USER'S OWN WORK -- userdata.db, the counterpart of the local index (index.db,
    Cache/cachedb.h).

    The index holds only what can be rebuilt from the images, and its failure policy is
    to move the file aside and start again. This file holds what NOTHING can rebuild,
    because the user made it:

      o collections, queries and places -- tables node and member (Datamodel/
        collectionstore.h)
      o the keyword vocabulary -- table vocab (Datamodel/keywordvocab.h): the hierarchy
        keywords are filed into, with synonyms and the export flag. It lived in index.db
        until schema 2 here, which is how a reset of the index could take it with it.
      o Audit Keywords' verdicts and excluded keywords -- tables audit_verdict and
        audit_skip (Datamodel/keywordauditstore.h), schema 3

    SO THIS FILE IS NEVER MOVED ASIDE. One it cannot open, or one written by a newer
    Winnow, leaves its features unavailable -- each panel greys with unavailableReason()
    -- and the file untouched for the user to recover.

    THE NAME. It was collections.db while it held only collections; db() renames an
    existing collections.db (with its -wal/-shm files) to userdata.db once, before
    opening it. If the rename fails the old file is opened where it is, so nothing is
    lost either way.

    GUI THREAD. One connection, opened on first use and kept. The stores that use it
    (CollectionStore, KeywordVocab) live on the GUI thread; CollectionStore's FileOps
    notifications hop there before they touch the database.
*/
class UserDb
{
    Q_DECLARE_TR_FUNCTIONS(UserDb)

public:
    static UserDb &instance();

    /*  Point at a file. Closes the open connection; the next db() opens the new one.
        Tests point it at a temp file; the app leaves the default,
        AppDataLocation/userdata.db. */
    void setPath(const QString &dbPath);
    QString path() const;

    /*  The open connection, or a closed QSqlDatabase when the file cannot be used. A
        failure is remembered rather than retried on every call; setPath clears it. */
    QSqlDatabase db();
    bool isOpen() const { return opened; }
    bool isAvailable() { return db().isOpen(); }
    QString unavailableReason() const { return lastError; }

    /*  The index file a fresh vocabulary is imported from (schema 2). Empty = ask the
        catalog for its file. Tests set it so they never touch AppDataLocation. */
    void setLegacyIndexPath(const QString &indexPath) { legacyIndexPath = indexPath; }

private:
    UserDb() = default;
    Q_DISABLE_COPY(UserDb)

    void adoptCollectionsDb(const QString &target);
    bool migrate(QSqlDatabase &d);
    bool importIndexVocab(QSqlDatabase &d);

    QString dbPath;
    QString legacyIndexPath;
    QString connName;
    QString lastError;
    bool opened = false;
    bool failed = false;
};

#endif // USERDB_H
