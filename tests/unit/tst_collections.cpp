#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Cache/devpreviewcache.h"
#include "Cache/pathkey.h"
#include "Datamodel/collectionstore.h"

/*
    tst_collections pins the store behind the Collections panel (Datamodel/
    collectionstore.h) and the two lookups G::CollectionsColumn is built from
    (MW::refreshCollectionMembership): the store's key -> ids, and the catalog's
    key -> the path a Library row holds.

    What matters most is what cannot be put back: a collection is the user's own work,
    so a delete must take exactly its subtree, a drag must never nest a collection
    inside itself, a move Winnow makes must carry membership with the image, and a file
    from a newer build must be LEFT ALONE rather than rebuilt the way the index is.
*/
class tst_collections : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void nodesComeParentsFirstInOrder();
    void reparentRefusesCyclesAndOtherKinds();
    void removeTakesTheSubtreeAndItsMembers();
    void membershipIsIdempotentAndCounted();
    void membershipIsDirectAndPerKind();
    void queriesKeepTheirDefinitionAndDuplicate();
    void aMoveFollowsTheImage();
    void aDeleteLeavesEveryCollection();
    void catalogMapsKeysToRowPaths();
    void aNewerFileIsLeftUntouched();

private:
    using Kind = CollectionStore::Kind;
    QTemporaryDir tmp;          // collections.db
    QTemporaryDir cacheTmp;     // index.db, for the catalog case
    QTemporaryDir images;
    int fileNo = 0;
    QString dbPath() const { return QDir(tmp.path()).absoluteFilePath("collections.db"); }
    QString img(const QString &name) const
    {
        return QDir(images.path()).absoluteFilePath(name);
    }
};

void tst_collections::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(tmp.isValid());
    QVERIFY(cacheTmp.isValid());
    QVERIFY(images.isValid());
}

void tst_collections::init()
{
    /*  A fresh file per case: setPath closes the old connection first. */
    CollectionStore::instance().setPath(QString());
    QFile::remove(dbPath());
    QFile::remove(dbPath() + "-wal");
    QFile::remove(dbPath() + "-shm");
    CollectionStore::instance().setPath(dbPath());
    QVERIFY(CollectionStore::instance().isAvailable());
}

void tst_collections::cleanupTestCase()
{
    CollectionStore::instance().setPath(QString());
    CacheDb::instance().closeThisThread();
}

void tst_collections::nodesComeParentsFirstInOrder()
{
    CollectionStore &s = CollectionStore::instance();
    const qint64 a = s.create(Kind::Collection, 0, "Birds");
    const qint64 b = s.create(Kind::Collection, 0, "Trips");
    const qint64 c = s.create(Kind::Collection, a, "Hawks");
    QVERIFY(a && b && c);
    // a Query of the same name lives in its own tree
    QVERIFY(s.create(Kind::Query, 0, "Birds"));

    const QVector<CollectionStore::Node> n = s.nodes(Kind::Collection);
    QCOMPARE(n.size(), 3);
    QCOMPARE(n.at(0).id, a);
    QCOMPARE(n.at(1).id, b);
    QCOMPARE(n.at(2).id, c);
    QCOMPARE(n.at(2).parent, a);
    QCOMPARE(s.nodes(Kind::Query).size(), 1);

    QVERIFY(!s.create(Kind::Collection, 0, "   "));        // no blank names
    QVERIFY(s.rename(b, "Travel"));
    QCOMPARE(s.node(b).name, QString("Travel"));
}

void tst_collections::reparentRefusesCyclesAndOtherKinds()
{
    CollectionStore &s = CollectionStore::instance();
    const qint64 a = s.create(Kind::Collection, 0, "A");
    const qint64 b = s.create(Kind::Collection, a, "B");
    const qint64 c = s.create(Kind::Collection, b, "C");
    const qint64 q = s.create(Kind::Query, 0, "Q");

    QVERIFY(s.wouldCycle(a, c));
    QVERIFY(!s.reparent(a, c));                 // into its own grandchild
    QVERIFY(!s.reparent(a, a));                 // onto itself
    QVERIFY(!s.reparent(c, q));                 // a collection under a query
    QCOMPARE(s.node(a).parent, qint64(0));

    QVERIFY(s.reparent(c, 0));                  // back to the top level
    QCOMPARE(s.node(c).parent, qint64(0));
    QVERIFY(s.reparent(a, c));                  // now legal: C is no longer under A
    QCOMPARE(s.node(a).parent, c);
    QCOMPARE(s.subtree(c).size(), 3);           // C, A, B
}

void tst_collections::removeTakesTheSubtreeAndItsMembers()
{
    CollectionStore &s = CollectionStore::instance();
    const qint64 a = s.create(Kind::Collection, 0, "A");
    const qint64 b = s.create(Kind::Collection, a, "B");
    const qint64 other = s.create(Kind::Collection, 0, "Other");
    QCOMPARE(s.addMembers(b, {img("1.nef")}), 1);
    QCOMPARE(s.addMembers(other, {img("1.nef")}), 1);

    QVERIFY(s.remove(a));
    QCOMPARE(s.nodes(Kind::Collection).size(), 1);
    QCOMPARE(s.node(b).id, qint64(0));
    // the image's membership elsewhere is untouched
    QCOMPARE(s.memberCounts(Kind::Collection).value(other), 1);
    QCOMPARE(s.memberCounts(Kind::Collection).value(b), 0);
}

void tst_collections::membershipIsIdempotentAndCounted()
{
    CollectionStore &s = CollectionStore::instance();
    const qint64 a = s.create(Kind::Collection, 0, "A");
    QCOMPARE(s.addMembers(a, {img("1.nef"), img("2.nef")}), 2);
    // the same image spelled another way is the same member
    QCOMPARE(s.addMembers(a, {img("1.nef"), QDir::toNativeSeparators(img("2.NEF"))}), 0);
    QCOMPARE(s.memberCounts(Kind::Collection).value(a), 2);
    QCOMPARE(s.removeMembers(a, {img("2.nef"), img("9.nef")}), 1);
    QCOMPARE(s.memberPaths({a}), QStringList{img("1.nef")});

    // a query holds no members
    const qint64 q = s.create(Kind::Query, 0, "Q");
    QCOMPARE(s.addMembers(q, {img("1.nef")}), 0);
}

void tst_collections::membershipIsDirectAndPerKind()
{
    /*  DIRECT membership only: the Filters category's parent item matches its own
        images, and the Collections panel's Opt+click is what adds the children. An image
        in two collections carries both ids. */
    CollectionStore &s = CollectionStore::instance();
    const qint64 a = s.create(Kind::Collection, 0, "A");
    const qint64 b = s.create(Kind::Collection, a, "B");
    const qint64 c = s.create(Kind::Collection, 0, "C");
    s.addMembers(a, {img("1.nef")});
    s.addMembers(b, {img("2.nef")});
    s.addMembers(c, {img("1.nef")});

    const QHash<QString, QStringList> m = s.membershipByKey(Kind::Collection);
    QCOMPARE(m.size(), 2);
    QStringList one = m.value(cachePathKey(img("1.nef")));
    one.sort();
    QStringList want{QString::number(a), QString::number(c)};
    want.sort();
    QCOMPARE(one, want);
    QCOMPARE(m.value(cachePathKey(img("2.nef"))), QStringList{QString::number(b)});
    QVERIFY(s.membershipByKey(Kind::Query).isEmpty());
}

void tst_collections::queriesKeepTheirDefinitionAndDuplicate()
{
    /*  A Query's definition is its JSON (Utilities/queryexpr.h), stored in node.definition
        and kept through an edit; a duplicate copies it (a collection's copies members). */
    CollectionStore &s = CollectionStore::instance();
    const QString def = R"({"root":{"all":[{"f":"rating","op":"ge","v":["3"]}]},"version":1})";
    const qint64 q = s.create(Kind::Query, 0, "Good ones", def);
    QVERIFY(q);
    QCOMPARE(s.node(q).definition, def);
    QVERIFY(s.setDefinition(q, "{}"));
    QCOMPARE(s.node(q).definition, QString("{}"));
    const qint64 copy = s.duplicate(q);
    QVERIFY(copy && copy != q);
    QCOMPARE(s.node(copy).definition, QString("{}"));
    QCOMPARE(s.node(copy).name, QString("Good ones copy"));

    const qint64 c = s.create(Kind::Collection, 0, "C");
    s.addMembers(c, {img("1.nef"), img("2.nef")});
    const qint64 cc = s.duplicate(c);
    QCOMPARE(s.memberCounts(Kind::Collection).value(cc), 2);
}

void tst_collections::aMoveFollowsTheImage()
{
    CollectionStore &s = CollectionStore::instance();
    const qint64 a = s.create(Kind::Collection, 0, "A");
    const qint64 b = s.create(Kind::Collection, 0, "B");
    s.addMembers(a, {img("old.nef")});
    s.addMembers(b, {img("old.nef"), img("new.nef")});

    s.onMoved(img("old.nef"), img("new.nef"));
    QCOMPARE(s.memberPaths({a}), QStringList{img("new.nef")});
    // B already held the destination: one membership, not two, and no failure
    QCOMPARE(s.memberPaths({b}), QStringList{img("new.nef")});
}

void tst_collections::aDeleteLeavesEveryCollection()
{
    CollectionStore &s = CollectionStore::instance();
    const qint64 a = s.create(Kind::Collection, 0, "A");
    const qint64 b = s.create(Kind::Collection, 0, "B");
    s.addMembers(a, {img("1.nef"), img("2.nef")});
    s.addMembers(b, {img("1.nef")});
    s.onDeleted(img("1.nef"));
    QCOMPARE(s.memberPaths({a, b}), QStringList{img("2.nef")});
}

void tst_collections::catalogMapsKeysToRowPaths()
{
    /*  The side table behind G::CollectionsColumn is keyed by the path a Library row was
        loaded with, which is image.path -- so the lookup must hand back exactly that
        spelling for a key, and nothing for a key the index does not hold. */
    DevPreviewCache::instance().setCacheDir(cacheTmp.path());
    Catalog &cat = Catalog::instance();
    cat.clear();

    QVector<CatalogRow> rows;
    for (const QString &name : {"a.nef", "B.nef"}) {
        const QString p = img(name);
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
        const QFileInfo fi(p);
        CatalogRow r;
        r.path = p;
        r.folder = fi.absoluteDir().path();
        r.filename = fi.fileName();
        r.ext = "nef";
        r.srcSize = fi.size();
        r.srcMtime = fi.lastModified().toSecsSinceEpoch();
        r.captured = QDateTime::fromSecsSinceEpoch(1600000000 + ++fileNo);
        rows << r;
    }
    QCOMPARE(cat.commit(rows), 2);

    const QString ka = cachePathKey(img("a.nef"));
    const QString kb = cachePathKey(img("B.nef"));
    const QHash<QString, QString> got =
        cat.pathsForKeys({ka, kb, cachePathKey(img("not-catalogued.nef"))});
    QCOMPARE(got.size(), 2);
    QCOMPARE(got.value(ka), img("a.nef"));
    QCOMPARE(got.value(kb), img("B.nef"));          // the file's own spelling, not the key
    QVERIFY(cat.pathsForKeys({}).isEmpty());
}

void tst_collections::aNewerFileIsLeftUntouched()
{
    /*  The index is moved aside and rebuilt when it cannot be migrated; collections
        cannot be rebuilt, so a file from a newer Winnow must survive an older one. */
    CollectionStore &s = CollectionStore::instance();
    QVERIFY(s.create(Kind::Collection, 0, "Keep me"));
    s.setPath(QString());                       // close it

    {
        QSqlDatabase d = QSqlDatabase::addDatabase("QSQLITE", "tst_collections_newer");
        d.setDatabaseName(dbPath());
        QVERIFY(d.open());
        QSqlQuery q(d);
        QVERIFY(q.exec("PRAGMA user_version = 99"));
        d.close();
    }
    QSqlDatabase::removeDatabase("tst_collections_newer");

    s.setPath(dbPath());
    QVERIFY(!s.isAvailable());
    QVERIFY(!s.unavailableReason().isEmpty());
    QVERIFY(QFile::exists(dbPath()));

    {
        QSqlDatabase d = QSqlDatabase::addDatabase("QSQLITE", "tst_collections_check");
        d.setDatabaseName(dbPath());
        QVERIFY(d.open());
        QSqlQuery q(d);
        QVERIFY(q.exec("SELECT name FROM node"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QString("Keep me"));
        d.close();
    }
    QSqlDatabase::removeDatabase("tst_collections_check");
}

QTEST_MAIN(tst_collections)
#include "tst_collections.moc"
