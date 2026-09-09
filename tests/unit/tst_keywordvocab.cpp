#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QSqlQuery>

#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Cache/devpreviewcache.h"
#include "Datamodel/keywordvocab.h"
#include "Metadata/keywordpaths.h"
#include "Main/global.h"

/*
    THE AUTHORED VOCABULARY'S TREE SURGERY, and above all the MERGE.

    Everything else this model does moves one node: rename changes a name, reparent moves
    a subtree somewhere its name is free, insertChild adds a leaf. Merge is the one
    operation that walks two trees at once and destroys part of one of them, and it is the
    one a real vocabulary needs most -- a library keyworded by two applications holds the
    same place twice ("Canada|BC|Vancouver Island" beside "Location|Canada|BC|Vancouver
    Island"), and folding one into the other is the only way out that keeps the tails.

    WHAT THESE CASES DEFEND is that the tails survive at any depth, that a colliding child
    merges rather than duplicating, that the source is really gone, and that exactly ONE
    pathChanged comes out -- because that signal raises a dialog that rewrites files, and
    one per node moved would raise it once per keyword in the branch.
*/
class tst_keywordvocab : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void mergeKeepsTailsAtEveryDepth();
    void mergeFoldsCollidingChildrenRatherThanDuplicating();
    void mergeEmitsOnePathChangeForTheWholeBranch();
    void mergeUnionsSynonyms();
    void mergeIsRefusedIntoItsOwnDescendant();
    void reparentWithNoCollisionIsStillAPlainMove();
    void aMoveWithNoSynonymsReachesTheDatabase();

private:
    /*  Create a path a node at a time, as the dock's own "add child" walk does. Returns
        the deepest node's index. */
    QModelIndex make(KeywordVocab &v, const QString &path);
    QStringList allPaths(const KeywordVocab &v) const;

    QTemporaryDir tmp;
};

QModelIndex tst_keywordvocab::make(KeywordVocab &v, const QString &path)
{
    QModelIndex parent;
    QString built;
    for (const QString &node : keywordNodes(path)) {
        built = built.isEmpty() ? node : built + '|' + node;
        const QModelIndex have = v.indexForPath(built);
        parent = have.isValid() ? have : v.insertChild(parent, node);
    }
    return parent;
}

QStringList tst_keywordvocab::allPaths(const KeywordVocab &v) const
{
    QStringList out;
    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        for (int i = 0; i < v.rowCount(parent); ++i) {
            const QModelIndex idx = v.index(i, 0, parent);
            out << idx.data(KeywordVocab::PathRole).toString();
            walk(idx);
        }
    };
    walk(QModelIndex());
    out.sort();
    return out;
}

void tst_keywordvocab::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(tmp.isValid());
    DevPreviewCache::instance().setCacheDir(tmp.path());
}

void tst_keywordvocab::cleanupTestCase()
{
    CacheDb::instance().closeThisThread();
}

void tst_keywordvocab::init()
{
    Catalog::instance().clear();
    QSqlQuery q(CacheDb::instance().db());
    QVERIFY(q.exec("DELETE FROM vocab"));
}

void tst_keywordvocab::mergeKeepsTailsAtEveryDepth()
{
/*
    THE CASE THE FEATURE EXISTS FOR. "Canada" is filed under "Location", where a "Canada"
    already lives. Everything beneath it has to arrive intact and at full depth -- a merge
    that flattened "Canada|BC|Nanaimo" to "Location|Canada|Nanaimo", or worse to
    "Location|Canada", would lose the place detail the user is trying to preserve.
*/
    KeywordVocab v;
    v.reload();

    make(v, "Location|Canada|BC|Victoria");
    make(v, "Canada|BC|Nanaimo");
    make(v, "Canada|Yukon|Whitehorse");

    const QModelIndex src = v.indexForPath("Canada");
    const QModelIndex dst = v.indexForPath("Location");
    QVERIFY(src.isValid() && dst.isValid());
    QVERIFY(v.wouldMerge(src, dst));

    QCOMPARE(v.reparentMerging(src, dst), QString("Location|Canada"));

    const QStringList paths = allPaths(v);
    QVERIFY(paths.contains("Location|Canada|BC|Nanaimo"));
    QVERIFY(paths.contains("Location|Canada|BC|Victoria"));
    QVERIFY(paths.contains("Location|Canada|Yukon|Whitehorse"));

    /*  The source branch is GONE, not left behind empty beside its own contents. */
    QVERIFY(!paths.contains("Canada"));
    QVERIFY(!v.indexForPath("Canada").isValid());
    QVERIFY(!v.indexForPath("Canada|BC").isValid());
}

void tst_keywordvocab::mergeFoldsCollidingChildrenRatherThanDuplicating()
{
/*
    "BC" exists on BOTH sides, so it must be merged in turn rather than moved -- a move
    would need a path that is already taken, which the unique index would refuse anyway.
    One BC afterwards, holding both cities.
*/
    KeywordVocab v;
    v.reload();

    make(v, "Location|Canada|BC|Victoria");
    make(v, "Canada|BC|Nanaimo");

    QCOMPARE(v.reparentMerging(v.indexForPath("Canada"), v.indexForPath("Location")),
             QString("Location|Canada"));

    const QStringList paths = allPaths(v);
    QCOMPARE(paths.count("Location|Canada|BC"), 1);
    QVERIFY(paths.contains("Location|Canada|BC|Victoria"));
    QVERIFY(paths.contains("Location|Canada|BC|Nanaimo"));

    /*  Survives a round trip through the database: the merge wrote rows, it did not just
        rearrange pointers. */
    KeywordVocab again;
    again.reload();
    QCOMPARE(allPaths(again), paths);
}

void tst_keywordvocab::mergeEmitsOnePathChangeForTheWholeBranch()
{
/*
    ONE signal, naming the TOP of the move. MW::keywordPathChanged turns each one into a
    dialog that offers to rewrite files, and MW::retagKeywordPath rewrites every
    descendant at its PREFIX -- so the top-level move already describes the whole subtree.
    One per node moved would put the dialog on screen once per keyword in the branch.
*/
    KeywordVocab v;
    v.reload();

    make(v, "Location|Canada|BC|Victoria");
    make(v, "Canada|BC|Nanaimo");
    make(v, "Canada|Yukon|Whitehorse");

    QSignalSpy spy(&v, &KeywordVocab::pathChanged);
    v.reparentMerging(v.indexForPath("Canada"), v.indexForPath("Location"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("Canada"));
    QCOMPARE(spy.at(0).at(1).toString(), QString("Location|Canada"));
}

void tst_keywordvocab::mergeUnionsSynonyms()
{
/*
    Two nodes that turn out to be the same keyword have between them every spelling the
    user recorded for it. Letting one side win would silently drop completions they had
    already set up.
*/
    KeywordVocab v;
    v.reload();

    const QModelIndex keep = make(v, "Location|Canada");
    const QModelIndex gone = make(v, "Canada");
    QVERIFY(v.setSynonyms(keep, {"CA"}));
    QVERIFY(v.setSynonyms(gone, {"Canuckistan", "ca"}));

    v.reparentMerging(v.indexForPath("Canada"), v.indexForPath("Location"));

    const QStringList syn =
        v.indexForPath("Location|Canada").data(KeywordVocab::SynonymsRole).toStringList();
    QVERIFY(syn.contains("CA"));
    QVERIFY(syn.contains("Canuckistan"));
    /*  Folded, so "ca" does not arrive as a second spelling of "CA". */
    QCOMPARE(syn.size(), 2);
}

void tst_keywordvocab::mergeIsRefusedIntoItsOwnDescendant()
{
/*
    Merging a branch into something inside itself is the cycle every mover here has to
    refuse, and it must be refused by the SAME test reparent uses or the two would
    disagree about what "beneath" means.
*/
    KeywordVocab v;
    v.reload();
    make(v, "Location|Canada|BC");

    const QModelIndex loc = v.indexForPath("Location");
    const QModelIndex bc  = v.indexForPath("Location|Canada|BC");
    QVERIFY(!v.wouldMerge(loc, bc));
    QCOMPARE(v.reparentMerging(loc, bc), QString());
    QVERIFY(v.indexForPath("Location|Canada|BC").isValid());
}

void tst_keywordvocab::reparentWithNoCollisionIsStillAPlainMove()
{
/*
    No counterpart in the destination means there is nothing to merge, and reparentMerging
    must hand straight over to reparent rather than growing a second implementation of the
    path rewrite for the two to drift apart in.
*/
    KeywordVocab v;
    v.reload();
    make(v, "Location|USA");
    make(v, "Canada|BC");

    QCOMPARE(v.reparentMerging(v.indexForPath("Canada"), v.indexForPath("Location")),
             QString("Location|Canada"));

    const QStringList paths = allPaths(v);
    QVERIFY(paths.contains("Location|Canada|BC"));
    QVERIFY(paths.contains("Location|USA"));
    QVERIFY(!paths.contains("Canada"));
}

void tst_keywordvocab::aMoveWithNoSynonymsReachesTheDatabase()
{
/*
    THE BUG THE MERGE FOUND, and it had nothing to do with merging.

    vocab.synonyms is "NOT NULL DEFAULT ''". QStringList::join returns a
    default-constructed -- NULL -- QString for an empty list, Qt binds a null QString as
    SQL NULL, and SQLite refuses the UPDATE. So every write of a node with NO SYNONYMS
    failed, which is very nearly every node in a real vocabulary.

    IT WAS INVISIBLE IN A RUNNING SESSION because the tree on screen is the in-memory one:
    rename changed the name and returned false without emitting pathChanged (so the
    photographs were never offered the retag), and reparent did not check the result at
    all, so a moved branch stayed moved until the next reload put it back.

    THE ASSERTION IS THE RELOAD. Checking the model in memory would have passed against
    the broken code; only re-reading what was actually stored can tell.
*/
    KeywordVocab v;
    v.reload();
    make(v, "Location");
    make(v, "Canada|BC|Nanaimo");
    QVERIFY(v.indexForPath("Canada").data(KeywordVocab::SynonymsRole)
                .toStringList().isEmpty());

    QVERIFY(v.reparent(v.indexForPath("Canada"), v.indexForPath("Location")));
    QVERIFY(v.rename(v.indexForPath("Location|Canada|BC"), "British Columbia"));

    KeywordVocab again;
    again.reload();
    const QStringList paths = allPaths(again);
    QVERIFY2(paths.contains("Location|Canada"), "the move must survive a reload");
    QVERIFY2(paths.contains("Location|Canada|British Columbia"),
             "the rename must survive a reload");
    QVERIFY2(paths.contains("Location|Canada|British Columbia|Nanaimo"),
             "a descendant rewritten by rewritePaths must survive a reload");
}

QTEST_MAIN(tst_keywordvocab)
#include "tst_keywordvocab.moc"
