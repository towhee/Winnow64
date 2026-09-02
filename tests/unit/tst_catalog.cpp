#include <QtTest>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Cache/pathkey.h"
#include "Cache/devpreviewcache.h"
#include "Metadata/keywordflatten.h"
#include "Main/global.h"

/*
    The catalog -- the local index behind cross-folder keyword and metadata search.

    The database is shared with the develop-preview index, so several of these test the
    BOUNDARY between the two tenants as much as the catalog itself: the schema migration
    must not disturb devpreview rows, and clearing one tenant must not clear the other.

    DevPreviewCache::setCacheDir is what points CacheDb at the sandbox; Catalog has no
    setter of its own precisely because it must never override a location someone else
    chose. That arrangement is itself pinned below.
*/
class tst_catalog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void schemaIsCurrentAndBothTenantsCoexist();
    void commitThenSearchByKeyword();
    void hierarchyReachesAncestors();
    void freeTextFindsTitleAndGear();
    void unchangedRowsAreSkipped();
    void sidecarEditForcesReindex();
    void pathSpellingDoesNotSplitTheRow();
    void keywordCategoryCountsImages();
    void staleOfReportsOnlyWhatChanged();
    void fetchFreshReturnsWhatNeedNotBeRead();
    void fetchFreshAndStaleOfAgreeExactly();
    void displayFieldsSurviveTheRoundTrip();
    void sweepDemotesMissingSource();
    void availabilityDistinguishesOfflineFromMissing();
    void onMovedFollowsTheImage();
    void onDeletedRemovesTheRow();
    void searchTextIsNotInjectable();
    void clearingPreviewsLeavesTheCatalog();
    void staleOfSkipsUnchangedOnRescan();
    void removedKeywordDisappearsFromCategory();
    void lightroomDoubleCollapsesToOneKeyword();
    void ambiguousKeywordIsReported();
    void excludeKeywordSeparatesTwoPlaces();
    void textSearchHonoursOrAndNot();
    void migrationFromVersionThreeMergesKeywords();
    void categoryItemsMatchWhatTheDatamodelWrites();
    void categoryItemsAreEmptyForColumnsTheIndexCannotAnswer();
    void genericIncludeAndExcludeNarrowTheSearch();
    void blankCategoryItemAccountsForEveryImage();

private:
    QString imagePath(const QString &name) const;
    /* A catalog row for a real file in the sandbox, with the freshness stamp filled in
       from disk the way DataModel::catalogRows does. */
    CatalogRow rowFor(const QString &name, const QStringList &keywords = {},
                      const QStringList &paths = {});

    QTemporaryDir tmp;          // stands in for the image folder
    QTemporaryDir cacheTmp;     // stands in for AppDataLocation/PreviewCache
};

QString tst_catalog::imagePath(const QString &name) const
{
    return QDir(tmp.path()).absoluteFilePath(name);
}

CatalogRow tst_catalog::rowFor(const QString &name, const QStringList &keywords,
                               const QStringList &paths)
{
    const QString p = imagePath(name);
    if (!QFile::exists(p)) {
        QFile f(p);
        f.open(QIODevice::WriteOnly);
        f.write("not really an image");
        f.close();
    }
    const QFileInfo fi(p);

    CatalogRow r;
    r.path = p;
    r.folder = fi.absoluteDir().path();
    r.filename = fi.fileName();
    r.ext = fi.suffix().toLower();
    r.srcSize = fi.size();
    r.srcMtime = fi.lastModified().toSecsSinceEpoch();
    r.captured = QDateTime::fromSecsSinceEpoch(1600000000);
    /* Flattened HERE because that is where the real callers do it -- both
       DataModel::catalogRows and CatalogScanner hand Catalog the output of
       flattenKeywords, never the two raw lists. A test that skipped this would be
       exercising a shape the app never produces. */
    r.keywords = flattenKeywords(keywords, paths);
    r.keywordPaths = paths;
    return r;
}

void tst_catalog::initTestCase()
{
    /* Keep every path this test touches inside the sandbox -- see tst_devpreview. */
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(tmp.isValid());
    QVERIFY(cacheTmp.isValid());
}

void tst_catalog::cleanupTestCase()
{
    /* Release the handle before QTemporaryDir removes the directory under it. */
    CacheDb::instance().closeThisThread();
}

void tst_catalog::init()
{
    DevPreviewCache &c = DevPreviewCache::instance();
    c.setCacheDir(cacheTmp.path());
    c.clear();
    Catalog::instance().clear();
}

void tst_catalog::schemaIsCurrentAndBothTenantsCoexist()
{
    /*  A TRIPWIRE, deliberately spelled as a literal. Bumping kSchemaVersion
        fails this case, which is the point: whoever bumps it has to come here
        and confirm that every tenant of the shared file still coexists at the
        new version, and add the new tenant's tables below. Version 5 added
        thumb (Cache/thumbcache.h); version 6 widened the image table with the
        fields a datamodel ROW displays; version 7 added the hierarchical
        keyword paths, which the flattening had made unrecoverable. */
    QCOMPARE(CacheDb::schemaVersion(), 7);
    QVERIFY(Catalog::instance().isAvailable());

    /* The catalog's tables were ADDED to the preview index's database, so both tenants
       must be usable in the same file at the same version. */
    QSqlDatabase db = CacheDb::instance().db();
    QVERIFY(db.isOpen());
    const auto tables = db.tables();
    for (const char *t : {"devpreview", "image", "keyword", "image_keyword",
                          "keyword_context", "image_fts", "thumb"})
        QVERIFY2(tables.contains(t), t);
}

void tst_catalog::commitThenSearchByKeyword()
{
    Catalog &cat = Catalog::instance();
    QCOMPARE(cat.commit({rowFor("a.nef", {"Heron", "BC"})}), 1);
    QCOMPARE(cat.count(), 1);

    CatalogQuery q;
    q.keywords = {"Heron"};
    QCOMPARE(cat.search(q), QStringList{imagePath("a.nef")});

    /* Case must not matter: the user picks "heron" out of a category list that may have
       been written by an application that capitalised it differently. */
    q.keywords = {"hErOn"};
    QCOMPARE(cat.search(q).size(), 1);

    q.keywords = {"Eagle"};
    QVERIFY(cat.search(q).isEmpty());
}

void tst_catalog::hierarchyReachesAncestors()
{
/*
    Why the hierarchy is read at all, now that it is FLATTENED. The image is tagged only
    with the leaf "Heron" in dc:subject; the ancestors "Fauna" and "Bird" exist nowhere
    but lr:hierarchicalSubject. Flattening makes each of them a keyword in its own right,
    so a search for a parent still finds the picture -- with no tree to walk.

    Note there is no "Fauna|Bird" keyword to search for any more. A PATH is not a name,
    and the flat vocabulary has only names.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("b.nef", {"Heron"}, {"Fauna|Bird|Heron"})});

    CatalogQuery q;
    for (const QString &k : {"Fauna", "Bird", "Heron"}) {
        q.keywords = {k};
        QVERIFY2(cat.search(q).size() == 1,
                 qPrintable("ancestor search failed for " + k));
    }

    /* Free text must agree with the category item, or the two halves of the UI would
       disagree about the same picture. */
    CatalogQuery t;
    t.text = "fauna";
    QCOMPARE(cat.search(t).size(), 1);
}

void tst_catalog::freeTextFindsTitleAndGear()
{
    Catalog &cat = Catalog::instance();
    CatalogRow r = rowFor("c.nef");
    r.title = "Low tide at dawn";
    r.creator = "R Hill";
    r.model = "NIKON Z 9";
    r.lens = "NIKKOR Z 100-400mm";
    cat.commit({r});

    CatalogQuery q;
    for (const QString &text : {"dawn", "tide", "nikon", "100-400", "Hill", "daw"}) {
        q.text = text;
        QVERIFY2(cat.search(q).size() == 1, qPrintable("free text failed for " + text));
    }

    q.text = "sunset";
    QVERIFY(cat.search(q).isEmpty());
}

void tst_catalog::unchangedRowsAreSkipped()
{
/*
    The opportunistic capture runs on EVERY folder change, so a user pacing between two
    folders would rewrite both indefinitely if this did not hold.
*/
    Catalog &cat = Catalog::instance();
    const CatalogRow r = rowFor("d.nef", {"Heron"});
    QCOMPARE(cat.commit({r}), 1);
    QCOMPARE(cat.commit({r}), 0);      // nothing changed -> nothing written
    QCOMPARE(cat.commit({r}), 0);
    QCOMPARE(cat.count(), 1);
}

void tst_catalog::sidecarEditForcesReindex()
{
/*
    The load-bearing case for a raw library: Lightroom edits keywords by rewriting the
    .xmp and NEVER touches the NEF, so a freshness check that looked only at the image
    would never notice the change.
*/
    Catalog &cat = Catalog::instance();
    CatalogRow r = rowFor("e.nef", {"Heron"});
    r.sidecarMtime = 1000;
    QCOMPARE(cat.commit({r}), 1);
    QCOMPARE(cat.commit({r}), 0);

    // the sidecar was rewritten, and the keyword changed with it
    r.sidecarMtime = 2000;
    r.keywords = QStringList{"Eagle"};
    QCOMPARE(cat.commit({r}), 1);

    CatalogQuery q;
    q.keywords = {"Eagle"};
    QCOMPARE(cat.search(q).size(), 1);
    /* The OLD keyword must be gone, not merely outranked -- writeKeywords replaces the
       links rather than adding to them, which is how a keyword deleted in Lightroom
       disappears here too. */
    q.keywords = {"Heron"};
    QVERIFY(cat.search(q).isEmpty());
}

void tst_catalog::pathSpellingDoesNotSplitTheRow()
{
/*
    The devPreview index's invariant, now for the catalog: the same file reaches Winnow
    spelled several ways, and a byte-exact index would catalogue it once per spelling.
*/
    Catalog &cat = Catalog::instance();
    CatalogRow a = rowFor("f.nef", {"Heron"});

    CatalogRow b = a;
    b.path = QDir(tmp.path()).absolutePath() + "//" + "f.nef";   // doubled separator
    CatalogRow c = a;
    c.path = a.path.toUpper();                                   // different case

    cat.commit({a});
    cat.commit({b});
    cat.commit({c});
    QCOMPARE(cat.count(), 1);
}

void tst_catalog::keywordCategoryCountsImages()
{
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("g1.nef", {"Heron"}),
                rowFor("g2.nef", {"Heron"}),
                rowFor("g3.nef", {"Eagle"})});

    int heron = -1, eagle = -1;
    for (const CatalogKeyword &k : cat.keywords()) {
        if (k.name == "Heron") heron = k.count;
        if (k.name == "Eagle") eagle = k.count;
    }
    QCOMPARE(heron, 2);
    QCOMPARE(eagle, 1);
}

void tst_catalog::staleOfReportsOnlyWhatChanged()
{
    Catalog &cat = Catalog::instance();
    const CatalogRow a = rowFor("h1.nef");
    CatalogRow b = rowFor("h2.nef");
    cat.commit({a, b});

    QVERIFY(cat.staleOf({a, b}).isEmpty());

    b.srcMtime += 60;               // the file changed on disk
    const auto stale = cat.staleOf({a, b});
    QCOMPARE(stale.size(), 1);
    QVERIFY(stale.contains(b.path));

    // never seen before
    const CatalogRow fresh = rowFor("h3.nef");
    QVERIFY(cat.staleOf({fresh}).contains(fresh.path));
}

void tst_catalog::fetchFreshReturnsWhatNeedNotBeRead()
{
    /*  THE INDEX AS A METADATA SOURCE. A row the catalog already holds, whose
        file has not changed, can be handed to the loader instead of being read
        off disk -- which is the whole point: a metadata read opens the file,
        walks its header and, for keywords, parses a sidecar XML document. */
    Catalog &cat = Catalog::instance();

    CatalogRow a = rowFor("fetch1.jpg", {"Heron"}, {"Wildlife|Birds|Heron"});
    a.title = "Great Blue";
    a.creator = "R Hill";
    a.make = "NIKON CORPORATION";
    a.model = "NIKON Z 9";
    a.lens = "NIKKOR Z 400mm f/4.5";
    a.iso = 800;
    a.aperture = 5.6;
    a.shutter = 0.0008;
    a.focalLength = 400;
    a.width = 8256;
    a.height = 5504;
    a.rating = 4;
    a.label = "Red";
    a.gpsCoord = "49.28,-123.12";
    QCOMPARE(cat.commit({a}), 1);

    const QHash<QString, CatalogRow> got = cat.fetchFresh({a});
    QCOMPARE(got.size(), 1);
    QVERIFY2(got.contains(a.path), "keyed by the path AS SUPPLIED");

    const CatalogRow &r = got.value(a.path);
    QCOMPARE(r.title, QString("Great Blue"));
    QCOMPARE(r.creator, QString("R Hill"));
    QCOMPARE(r.model, QString("NIKON Z 9"));
    QCOMPARE(r.lens, QString("NIKKOR Z 400mm f/4.5"));
    QCOMPARE(r.iso, 800);
    QCOMPARE(r.aperture, 5.6);
    QCOMPARE(r.focalLength, 400.0);
    QCOMPARE(r.width, 8256);
    QCOMPARE(r.height, 5504);
    QCOMPARE(r.rating, 4);
    QCOMPARE(r.label, QString("Red"));
    QCOMPARE(r.gpsCoord, QString("49.28,-123.12"));

    /*  The keywords are the expensive half of a metadata read -- they live in
        the sidecar, not the file header -- so getting them back is most of the
        saving. Flat, as everything downstream of schema 4 expects. */
    QSet<QString> kws(r.keywords.begin(), r.keywords.end());
    QVERIFY2(kws.contains("Heron"), "the leaf keyword did not come back");
    QVERIFY2(kws.contains("Wildlife"), "an ancestor did not come back");
    QVERIFY2(kws.contains("Birds"), "an ancestor did not come back");

    /*  A file that changed is ABSENT rather than stale: it is the gap the
        reader still has to fill. */
    CatalogRow changed = a;
    changed.srcMtime += 1;
    QVERIFY2(cat.fetchFresh({changed}).isEmpty(),
             "a changed file must not be answered from the index");

    // and one that was never catalogued at all
    QVERIFY(cat.fetchFresh({rowFor("never-catalogued.jpg")}).isEmpty());
}

void tst_catalog::fetchFreshAndStaleOfAgreeExactly()
{
    /*  THE INVARIANT THAT MATTERS MOST HERE. staleOf tells the scanner what to
        INDEX; fetchFresh tells the loader what it need not READ. They apply the
        same freshness stamp from opposite sides, and if they ever disagree an
        image could be both skipped by the loader and ignored by the scanner --
        invisible, and permanently so, because nothing would revisit it until
        its file changed on disk. */
    Catalog &cat = Catalog::instance();

    const CatalogRow a = rowFor("agree1.jpg", {"A"});
    const CatalogRow b = rowFor("agree2.jpg", {"B"});
    const CatalogRow c = rowFor("agree3.jpg", {"C"});
    /*  d differs ONLY in its sidecar stamp -- a keyword edited in Lightroom,
        which rewrites the .xmp and never touches the raw. It is here because
        the first version of this case varied only srcSize, and an injected
        fetchFresh that ignored sidecarMtime entirely still passed: the two
        sides can disagree on a field the test never varies. */
    CatalogRow d = rowFor("agree4.jpg", {"D"});
    d.sidecarMtime = 1600000000;
    const CatalogRow c2 = c;

    QCOMPARE(cat.commit({a, b, d}), 3);    // c is never catalogued

    CatalogRow bChanged = b;
    bChanged.srcSize += 10;                // b has been edited since
    CatalogRow dResaved = d;
    dResaved.sidecarMtime += 1;            // only the sidecar moved

    const QList<CatalogRow> candidates { a, bChanged, c2, dResaved };
    const QSet<QString> stale = cat.staleOf(candidates);
    const QHash<QString, CatalogRow> fresh = cat.fetchFresh(candidates);

    for (const CatalogRow &r : candidates) {
        const bool isStale = stale.contains(r.path);
        const bool isFresh = fresh.contains(r.path);
        QVERIFY2(isStale != isFresh,
                 qPrintable(QString("%1: stale=%2 fresh=%3 -- the two must partition")
                                .arg(r.filename).arg(isStale).arg(isFresh)));
    }
    QVERIFY(fresh.contains(a.path));
    QVERIFY(stale.contains(bChanged.path));
    QVERIFY(stale.contains(c.path));
    QVERIFY2(stale.contains(dResaved.path),
             "a re-saved sidecar must make the row stale on both sides");
    QVERIFY2(!fresh.contains(dResaved.path),
             "a re-saved sidecar must not be answered from the index");
}

void tst_catalog::displayFieldsSurviveTheRoundTrip()
{
    /*  SCHEMA 6: the catalog became the persistent form of a ROW, not just a
        search index. These are the fields a row DISPLAYS that a search never
        needed, and they exist because serving a row's metadata from the catalog
        was fingerprinted against reading it from the file and these are exactly
        what diverged.

        orientation is the one that mattered: it drives rotation, so a row
        served without it shows the picture on its side. The _original values
        matter differently -- they are how an edit is reverted and how a sidecar
        write decides whether anything actually changed, so a row that came back
        without them would make Winnow think every image had been edited. */
    Catalog &cat = Catalog::instance();

    CatalogRow a = rowFor("display1.jpg", {"X"});
    a.orientation = 6;
    a.exposureComp = "-0.7";
    a.focusX = 0.25;
    a.focusY = 0.75;
    a.email = "someone@example.com";
    a.url = "https://example.com";
    a._rating = "3";
    a._label = "Blue";
    a._creator = "Original Creator";
    a._title = "Original Title";
    a._copyright = "(c) 2026";
    a._email = "orig@example.com";
    a._url = "https://orig.example.com";
    a.developed = true;
    a.devPreviewKey = "abc123";
    QCOMPARE(cat.commit({a}), 1);

    const QHash<QString, CatalogRow> got = cat.fetchFresh({a});
    QCOMPARE(got.size(), 1);
    const CatalogRow &r = got.value(a.path);

    QCOMPARE(r.orientation, 6);
    QCOMPARE(r.exposureComp, QString("-0.7"));
    QCOMPARE(r.focusX, 0.25);
    QCOMPARE(r.focusY, 0.75);
    QCOMPARE(r.email, QString("someone@example.com"));
    QCOMPARE(r.url, QString("https://example.com"));
    QCOMPARE(r._rating, QString("3"));
    QCOMPARE(r._label, QString("Blue"));
    QCOMPARE(r._creator, QString("Original Creator"));
    QCOMPARE(r._title, QString("Original Title"));
    QCOMPARE(r._copyright, QString("(c) 2026"));
    QCOMPARE(r._email, QString("orig@example.com"));
    QCOMPARE(r._url, QString("https://orig.example.com"));
    QCOMPARE(r.developed, true);
    QCOMPARE(r.devPreviewKey, QString("abc123"));

    /*  An UPDATE has to carry them too, not just an insert -- the two statements
        bind the same sequence and it would be easy to extend one and not the
        other. */
    CatalogRow b = a;
    b.srcMtime += 1;                    // force the update path
    b.orientation = 8;
    b._title = "Changed";
    QCOMPARE(cat.commit({b}), 1);
    const CatalogRow r2 = cat.fetchFresh({b}).value(b.path);
    QCOMPARE(r2.orientation, 8);
    QCOMPARE(r2._title, QString("Changed"));

    /*  THE HIERARCHICAL PATHS COME BACK VERBATIM (schema 7). Schema 4 flattened
        the hierarchy to node names on purpose -- the flat vocabulary is what is
        searched and filtered -- but the original "A|B|C" spelling is a separate
        fact that cannot be rebuilt from the flat form, and a row's SEARCHABLE
        TEXT is built from it. Without this a row served from the catalog
        searched differently from the same row read from its file. */
    CatalogRow k = rowFor("paths1.jpg", {"Heron"},
                          {"Wildlife|Birds|Heron", "Places|Canada|BC"});
    QCOMPARE(cat.commit({k}), 1);
    const CatalogRow kb = cat.fetchFresh({k}).value(k.path);
    QCOMPARE(kb.keywordPaths.size(), 2);
    QVERIFY(kb.keywordPaths.contains("Wildlife|Birds|Heron"));
    QVERIFY(kb.keywordPaths.contains("Places|Canada|BC"));
    /*  And the flat vocabulary is still the flat one -- the two representations
        coexist without either becoming the other. */
    QVERIFY(kb.keywords.contains("Heron"));
    QVERIFY(kb.keywords.contains("Wildlife"));
}

void tst_catalog::sweepDemotesMissingSource()
{
    Catalog &cat = Catalog::instance();
    const CatalogRow r = rowFor("i.nef", {"Heron"});
    cat.commit({r});

    CatalogQuery q;
    q.keywords = {"Heron"};
    QCOMPARE(cat.search(q).size(), 1);

    QVERIFY(QFile::remove(r.path));
    QCOMPARE(cat.sweep(), 1);

    /* Demoted, not deleted: the row survives so the image can be promoted again if the
       file comes back, but a search stops offering something that cannot be loaded. */
    QVERIFY(cat.search(q).isEmpty());
    QCOMPARE(cat.count(), 1);
    q.includeMissing = true;
    QCOMPARE(cat.search(q).size(), 1);
}

void tst_catalog::availabilityDistinguishesOfflineFromMissing()
{
    /*  OFFLINE IS NOT MISSING, and conflating them is what made the catalog hide
        rows instead of explaining them. An ejected card and a deleted file look
        identical from inside -- the file is not there either way -- and the only
        thing that tells them apart is whether the VOLUME is mounted.

        The sweep already depends on that distinction: it demotes a row only when
        the volume is mounted and the file is still gone, so that unplugging a
        drive is not read as a mass deletion. Nothing could ASK for it, though,
        so the catalog's only answer to "can I open this" was to drop the row
        from the results -- which for browsing is the wrong answer. */
    Catalog &cat = Catalog::instance();

    const CatalogRow here = rowFor("avail-present.jpg");
    const CatalogRow gone = rowFor("avail-gone.jpg");
    QCOMPARE(cat.commit({here, gone}), 2);

    /*  Delete one and sweep: the temp dir is on the boot volume, which is always
        mounted, so this is the MISSING case by construction. */
    QVERIFY(QFile::remove(gone.path));
    QCOMPARE(cat.sweep(), 1);

    const auto avail = cat.availabilityOf({here.path, gone.path,
                                           imagePath("never-indexed.jpg")});
    QCOMPARE(avail.value(here.path), Catalog::Availability::Present);
    QCOMPARE(avail.value(gone.path), Catalog::Availability::Missing);

    /*  A path the catalog does not know is ABSENT from the result, not reported
        Missing: "not indexed" is a different statement from "indexed and gone",
        and a caller that cannot tell them apart would badge every uncatalogued
        image as deleted. */
    QVERIFY2(!avail.contains(imagePath("never-indexed.jpg")),
             "an unindexed path must not be reported as Missing");

    /*  The OFFLINE case is asserted through the vol column directly, because a
        test cannot unmount a volume. A row whose recorded volume is not in the
        mount table must read Offline EVEN THOUGH it is also marked not live --
        the volume question is asked first, deliberately: while the disk is away
        there is no way to know whether the file is still gone, and "that disk
        isn't plugged in" is the thing the user can act on. */
    QSqlQuery u(CacheDb::instance().db());
    u.prepare("UPDATE image SET vol = ? WHERE pathkey = ?");
    u.addBindValue("/Volumes/NotMountedAnywhere");
    u.addBindValue(cachePathKey(gone.path));
    QVERIFY(u.exec());

    const auto avail2 = cat.availabilityOf({gone.path});
    QCOMPARE(avail2.value(gone.path), Catalog::Availability::Offline);
}

void tst_catalog::onMovedFollowsTheImage()
{
    Catalog &cat = Catalog::instance();
    const CatalogRow r = rowFor("j.nef", {"Heron"});
    cat.commit({r});

    const QString dst = imagePath("j-renamed.nef");
    QVERIFY(QFile::rename(r.path, dst));
    cat.onMoved(r.path, dst);

    CatalogQuery q;
    q.keywords = {"Heron"};
    QCOMPARE(cat.search(q), QStringList{dst});
    QCOMPARE(cat.count(), 1);
}

void tst_catalog::onDeletedRemovesTheRow()
{
    Catalog &cat = Catalog::instance();
    const CatalogRow r = rowFor("k.nef", {"Heron"});
    cat.commit({r});
    QCOMPARE(cat.count(), 1);

    cat.onDeleted(r.path);
    QCOMPARE(cat.count(), 0);

    /* A delete Winnow performed itself is certain, so the row goes rather than being
       demoted -- and the full-text row must go with it, or the search would still match
       on text belonging to an image that no longer exists. */
    CatalogQuery q;
    q.text = "heron";
    q.includeMissing = true;
    QVERIFY(cat.search(q).isEmpty());
}

void tst_catalog::searchTextIsNotInjectable()
{
/*
    The search box is user text and the catalog shares its file with the preview index, so
    a query that pasted text into SQL would put the previews one apostrophe away from
    damage. Every value is bound; these must all return quietly rather than throw, break
    the connection, or destroy anything.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("l.nef", {"Heron"})});

    const QStringList nasties = {
        "'; DROP TABLE image; --",
        "\" OR 1=1 --",
        "O'Brien",
        "100-400mm f/4.5",
        "*",
        "((("
    };
    for (const QString &nasty : nasties) {
        CatalogQuery q;
        q.text = nasty;
        cat.search(q);                 // must not crash or corrupt
    }

    QCOMPARE(cat.count(), 1);          // the table is still there
    CatalogQuery ok;
    ok.keywords = {"Heron"};
    QCOMPARE(cat.search(ok).size(), 1);
}

void tst_catalog::clearingPreviewsLeavesTheCatalog()
{
/*
    The two tenants share one file. "Clear preview cache" in preferences must not silently
    throw away the catalog with it -- rebuilding that means rescanning the library.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("m.nef", {"Heron"})});
    QCOMPARE(cat.count(), 1);

    DevPreviewCache &c = DevPreviewCache::instance();
    c.put(imagePath("m.nef"), "recipe1", QByteArray(1024, 'x'));
    QCOMPARE(c.count(), 1);

    c.clear();
    QCOMPARE(c.count(), 0);
    QCOMPARE(cat.count(), 1);          // the catalog is untouched

    /* And the reverse: clearing the catalog leaves the previews alone. */
    c.put(imagePath("m.nef"), "recipe1", QByteArray(1024, 'x'));
    cat.clear();
    QCOMPARE(cat.count(), 0);
    QCOMPARE(c.count(), 1);
}

void tst_catalog::staleOfSkipsUnchangedOnRescan()
{
/*
    What makes "Scan Now" cheap enough to offer as a button: a rescan of an unchanged
    root must cost one stat per file and NO parsing. The scanner asks staleOf before it
    opens anything, so an empty answer here is the difference between a rescan taking
    seconds and taking hours.
*/
    Catalog &cat = Catalog::instance();
    QList<CatalogRow> folder;
    for (int i = 0; i < 20; ++i)
        folder << rowFor(QString("scan%1.nef").arg(i), {"Heron"});

    QCOMPARE(cat.commit(QVector<CatalogRow>(folder.begin(), folder.end())), 20);
    /* The second pass over the same folder finds nothing to do. */
    QVERIFY(cat.staleOf(folder).isEmpty());

    /* Touch one sidecar and only that one comes back. */
    folder[7].sidecarMtime += 1;
    const auto stale = cat.staleOf(folder);
    QCOMPARE(stale.size(), 1);
    QVERIFY(stale.contains(folder[7].path));
}

void tst_catalog::removedKeywordDisappearsFromCategory()
{
/*
    A keyword deleted in Lightroom must stop being offered here. The keyword VOCABULARY
    row survives (another image may still use it), but this image's link is replaced, so
    the category item count drops and a search stops returning it.
*/
    Catalog &cat = Catalog::instance();
    CatalogRow a = rowFor("fa.nef", {"Heron", "BC"});
    CatalogRow b = rowFor("fb.nef", {"Heron"});
    cat.commit({a, b});

    auto countOf = [&cat](const QString &name) {
        for (const CatalogKeyword &k : cat.keywords())
            if (k.name == name) return k.count;
        return -1;
    };
    QCOMPARE(countOf("Heron"), 2);
    QCOMPARE(countOf("BC"), 1);

    // the user removed "BC" from fa.nef and Lightroom rewrote the sidecar
    a.keywords = QStringList{"Heron"};
    a.sidecarMtime += 1;
    QCOMPARE(cat.commit({a}), 1);

    QCOMPARE(countOf("Heron"), 2);
    QCOMPARE(countOf("BC"), 0);

    CatalogQuery q;
    q.keywords = {"BC"};
    QVERIFY(cat.search(q).isEmpty());
}


void tst_catalog::lightroomDoubleCollapsesToOneKeyword()
{
/*
    THE DEFECT THIS WHOLE CHANGE EXISTS FOR. Lightroom writes the same tag twice: the
    leaf
    into dc:subject and the full path into lr:hierarchicalSubject. Schema 3 keyed a
    keyword on (path, name) and so stored both forms, which put "Heron" in the category
    list TWICE with its image count split between the two entries.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("lr1.nef", {"Heron"}, {"Fauna|Bird|Heron"}),
                rowFor("lr2.nef", {"Heron"}, {"Fauna|Bird|Heron"})});

    int seen = 0, count = -1;
    for (const CatalogKeyword &k : cat.keywords()) {
        if (keywordFold(k.name) == "heron") { ++seen; count = k.count; }
    }
    QCOMPARE(seen, 1);          // one entry, not two
    QCOMPARE(count, 2);         // holding BOTH images, not one each

    /* And the ancestors are ordinary keywords beside it. */
    CatalogQuery q;
    q.keywords = {"Fauna"};
    QCOMPARE(cat.search(q).size(), 2);
}

void tst_catalog::ambiguousKeywordIsReported()
{
/*
    What flattening genuinely loses, and the one thing the docks mark: a name used under
    more than one parent means more than one thing.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("bc.nef", {}, {"Location|Canada|BC|Vancouver"}),
                rowFor("wa.nef", {}, {"Location|USA|Washington|Vancouver"}),
                rowFor("hn.nef", {}, {"Fauna|Bird|Heron"})});

    const QSet<QString> ambiguous = cat.ambiguousKeywords();
    QVERIFY(ambiguous.contains("vancouver"));       // two parents: BC and Washington
    QVERIFY(!ambiguous.contains("heron"));          // only ever under Bird
    QVERIFY(!ambiguous.contains("location"));       // a root, no parent at all

    /* The category item carries the parents so the panel can name them in a tooltip --
       knowing a word is ambiguous is not much use without knowing what the choices
       are. */
    for (const CatalogKeyword &k : cat.keywords()) {
        if (keywordFold(k.name) != "vancouver") continue;
        QCOMPARE(k.contexts.size(), 2);
        QVERIFY(k.contexts.contains("BC"));
        QVERIFY(k.contexts.contains("Washington"));
    }
}

void tst_catalog::excludeKeywordSeparatesTwoPlaces()
{
/*
    How an ambiguous keyword is resolved without a hierarchy: include the name, exclude
    the parent you do not want. This is the headline case for filter exclusion.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("x-bc.nef", {}, {"Location|Canada|BC|Vancouver"}),
                rowFor("x-wa.nef", {}, {"Location|USA|Washington|Vancouver"})});

    CatalogQuery q;
    q.keywords = {"Vancouver"};
    QCOMPARE(cat.search(q).size(), 2);              // both, as the name is ambiguous

    q.excludeKeywords = {"USA"};
    QCOMPARE(cat.search(q), QStringList{imagePath("x-bc.nef")});

    q.excludeKeywords = {"Canada"};
    QCOMPARE(cat.search(q), QStringList{imagePath("x-wa.nef")});

    /* Excluding something no image carries changes nothing -- an exclusion subtracts,
       so an empty subtraction is not an empty result. */
    q.excludeKeywords = {"Mexico"};
    QCOMPARE(cat.search(q).size(), 2);
}

void tst_catalog::textSearchHonoursOrAndNot()
{
/*
    The shared grammar (Utilities/searchterms.h). These are the queries that used to mean
    different things in the two search boxes -- to the datamodel's contains(), "heron OR
    eagle" was one literal string.
*/
    Catalog &cat = Catalog::instance();
    CatalogRow a = rowFor("t-heron.nef", {"Heron"});
    a.title = "Low tide";
    CatalogRow b = rowFor("t-eagle.nef", {"Eagle"});
    b.title = "High tide";
    cat.commit({a, b});

    CatalogQuery q;
    q.text = "heron OR eagle";
    QCOMPARE(cat.search(q).size(), 2);

    q.text = "tide -heron";
    QCOMPARE(cat.search(q), QStringList{imagePath("t-eagle.nef")});

    q.text = "tide NOT eagle";
    QCOMPARE(cat.search(q), QStringList{imagePath("t-heron.nef")});

    /* An all-negative query has no positive to subtract from, which FTS5's own NOT
       operator cannot express -- Catalog::search applies it as a separate NOT EXISTS. */
    q.text = "-heron";
    QCOMPARE(cat.search(q), QStringList{imagePath("t-eagle.nef")});

    q.text = "\"low tide\"";
    QCOMPARE(cat.search(q), QStringList{imagePath("t-heron.nef")});
}

void tst_catalog::migrationFromVersionThreeMergesKeywords()
{
/*
    An EXISTING catalog must migrate in place rather than being rebuilt: rebuilding means
    re-parsing a whole library to recover facts the database already holds.

    A version 3 file is built here by hand, in the shape schema 3 actually wrote -- one
    keyword row per hierarchy NODE with its path, PLUS a path-less row for the flat
    dc:subject leaf, with the image linked to all of them. After migration that image must
    have exactly the same keyword VOCABULARY, with the duplicate leaf merged away and the
    parent relationships preserved as contexts.
*/
    /* A separate database, so the migration is exercised from 3 and not from scratch. */
    QTemporaryDir v3Dir;
    QVERIFY(v3Dir.isValid());
    const QString dbPath = QDir(v3Dir.path()).absoluteFilePath("index.db");

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "v3fixture");
        db.setDatabaseName(dbPath);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE image (id INTEGER PRIMARY KEY, pathkey TEXT NOT NULL,"
                       " path TEXT NOT NULL, folder TEXT NOT NULL, vol TEXT NOT NULL"
                       " DEFAULT '', filename TEXT NOT NULL DEFAULT '', ext TEXT NOT NULL"
                       " DEFAULT '', srcsize INTEGER NOT NULL DEFAULT 0, srcmtime INTEGER"
                       " NOT NULL DEFAULT 0, sidecarmtime INTEGER NOT NULL DEFAULT 0,"
                       " indexed INTEGER NOT NULL DEFAULT 0, live INTEGER NOT NULL"
                       " DEFAULT 1, captured INTEGER, rating INTEGER NOT NULL DEFAULT 0,"
                       " label TEXT NOT NULL DEFAULT '', pick INTEGER NOT NULL DEFAULT 0,"
                       " title TEXT NOT NULL DEFAULT '', creator TEXT NOT NULL DEFAULT '',"
                       " copyright TEXT NOT NULL DEFAULT '', make TEXT NOT NULL DEFAULT '',"
                       " model TEXT NOT NULL DEFAULT '', lens TEXT NOT NULL DEFAULT '',"
                       " iso INTEGER NOT NULL DEFAULT 0, aperture REAL NOT NULL DEFAULT 0,"
                       " shutter REAL NOT NULL DEFAULT 0, focallength REAL NOT NULL"
                       " DEFAULT 0, width INTEGER NOT NULL DEFAULT 0, height INTEGER NOT"
                       " NULL DEFAULT 0, gpscoord TEXT NOT NULL DEFAULT '')"));
        QVERIFY(q.exec("CREATE UNIQUE INDEX image_pathkey ON image(pathkey)"));
        QVERIFY(q.exec("CREATE TABLE keyword (id INTEGER PRIMARY KEY, name TEXT NOT NULL,"
                       " namefold TEXT NOT NULL, path TEXT NOT NULL DEFAULT '',"
                       " pathfold TEXT NOT NULL DEFAULT '',"
                       " parent INTEGER REFERENCES keyword(id) ON DELETE SET NULL)"));
        QVERIFY(q.exec("CREATE UNIQUE INDEX keyword_key ON keyword(pathfold, namefold)"));
        QVERIFY(q.exec("CREATE INDEX keyword_name ON keyword(namefold)"));
        QVERIFY(q.exec("CREATE TABLE image_keyword (image_id INTEGER NOT NULL REFERENCES"
                       " image(id) ON DELETE CASCADE, keyword_id INTEGER NOT NULL"
                       " REFERENCES keyword(id) ON DELETE CASCADE,"
                       " PRIMARY KEY (image_id, keyword_id)) WITHOUT ROWID"));
        QVERIFY(q.exec("CREATE VIRTUAL TABLE image_fts USING fts5(keywords, title,"
                       " creator, copyright, gear, filename,"
                       " tokenize = 'unicode61 remove_diacritics 2')"));
        QVERIFY(q.exec("INSERT INTO image (id, pathkey, path, folder) VALUES"
                       " (1, '/lib/a.nef', '/lib/a.nef', '/lib')"));

        /* Exactly what schema 3 wrote for dc:subject = {Heron},
           lr:hierarchicalSubject = {Fauna|Bird|Heron}: three hierarchy rows wired by
           parent, and a FOURTH path-less row for the same leaf. */
        QVERIFY(q.exec("INSERT INTO keyword (id,name,namefold,path,pathfold,parent) VALUES"
                       " (1,'Fauna','fauna','Fauna','fauna',NULL),"
                       " (2,'Bird','bird','Fauna|Bird','fauna|bird',1),"
                       " (3,'Heron','heron','Fauna|Bird|Heron','fauna|bird|heron',2),"
                       " (4,'Heron','heron','','',NULL)"));
        QVERIFY(q.exec("INSERT INTO image_keyword (image_id, keyword_id) VALUES"
                       " (1,1),(1,2),(1,3),(1,4)"));
        QVERIFY(q.exec("PRAGMA user_version = 3"));
        db.close();
    }
    QSqlDatabase::removeDatabase("v3fixture");

    /* Open it as the app would. Migration runs on first use. */
    CacheDb::instance().setPath(dbPath);
    Catalog &cat = Catalog::instance();
    QVERIFY2(cat.isAvailable(), "a version 3 file must migrate, not be moved aside");

    /* Scoped, so no QSqlDatabase or QSqlQuery is still alive when the connection is
       closed below -- Qt warns that the connection "is still in use" otherwise, and a
       warning nobody can act on is worse than no output at all. */
    {
        QSqlDatabase db = CacheDb::instance().db();
        QSqlQuery q(db);
        QVERIFY(q.exec("PRAGMA user_version") && q.next());
        /*  Against the CURRENT version, not a literal: what this case is about
            is that a version 3 file migrates all the way UP and keeps its
            keywords, not which number it stops at. The literal belongs in the
            tripwire above, where changing it is the deliberate act. */
        QCOMPARE(q.value(0).toInt(), CacheDb::schemaVersion());

        /* The duplicate leaf is gone: three keywords, not four. */
        QVERIFY(q.exec("SELECT COUNT(*) FROM keyword") && q.next());
        QCOMPARE(q.value(0).toInt(), 3);

        /* The image still carries all three, and no link was lost or doubled. */
        QVERIFY(q.exec("SELECT COUNT(*) FROM image_keyword WHERE image_id = 1")
                && q.next());
        QCOMPARE(q.value(0).toInt(), 3);

        QStringList names;
        for (const CatalogKeyword &k : cat.keywords()) names << keywordFold(k.name);
        names.sort();
        QCOMPARE(names, (QStringList{"bird", "fauna", "heron"}));

        /* The hierarchy survives as contexts, which is what ambiguity detection needs. */
        QVERIFY(q.exec("SELECT COUNT(*) FROM keyword_context") && q.next());
        QCOMPARE(q.value(0).toInt(), 2);    // Bird under Fauna, Heron under Bird
    }

    /* Point the shared database back at the sandbox EXPLICITLY.

       DevPreviewCache::setCacheDir cannot do it: it early-returns when the directory it
       is given is the one it already holds, which after this test it is -- so the
       database would silently stay pointed at the v3 fixture, and every test after this
       one would run against a file QTemporaryDir is about to delete. That failure looks
       like "commit did nothing", which is a long way from its cause. */
    CacheDb::instance().closeThisThread();
    CacheDb::instance().setPath(QDir(cacheTmp.path()).absoluteFilePath("index.db"));
}


void tst_catalog::categoryItemsMatchWhatTheDatamodelWrites()
{
/*
    The Find dock shows ONE category list and the user cannot tell which scope produced
    it,
    so a value the catalog offers must be spelled exactly as DataModel spells the same
    value -- otherwise checking "NEF" in Catalog would mean nothing in Folders, and the
    scope switch would quietly change the question.
*/
    Catalog &cat = Catalog::instance();
    CatalogRow a = rowFor("fa1.nef");
    a.model = "NIKON Z 9";
    a.lens = "NIKKOR Z 100-400mm";
    a.rating = 3;
    a.pick = true;
    a.focalLength = 400;
    a.captured = QDateTime(QDate(2024, 6, 15), QTime(9, 30), QTimeZone::utc());
    CatalogRow b = rowFor("fb1.jpg");
    b.model = "NIKON Z 9";
    b.rating = 0;
    b.pick = false;
    b.captured = QDateTime(QDate(2024, 6, 15), QTime(10, 0), QTimeZone::utc());
    cat.commit({a, b});

    /* Type is the suffix UPPER-cased, as DataModel::addFileDataForRow writes it. */
    const QMap<QString, int> types = cat.categoryItems(G::TypeColumn);
    QCOMPARE(types.value("NEF"), 1);
    QCOMPARE(types.value("JPG"), 1);

    /* Year "yyyy" and Day "yyyy-MM-dd", as addMetadataForItem writes them. */
    QCOMPARE(cat.categoryItems(G::YearColumn).value("2024"), 2);
    QCOMPARE(cat.categoryItems(G::DayColumn).value("2024-06-15"), 2);

    /* Pick is the WORDS, not a boolean. */
    const QMap<QString, int> picks = cat.categoryItems(G::PickColumn);
    QCOMPARE(picks.value("Picked"), 1);
    QCOMPARE(picks.value("Unpicked"), 1);

    /* Rating is the digit as text, and unrated is the EMPTY key rather than "0" -- the
       blank is a category item in its own right so the category adds up. */
    const QMap<QString, int> ratings = cat.categoryItems(G::RatingColumn);
    QCOMPARE(ratings.value("3"), 1);
    QCOMPARE(ratings.value(""), 1);
    QVERIFY(!ratings.contains("0"));

    /* Focal length carries no trailing ".0", or "400" and "400.0" would look like two
       different lenses' worth of category. */
    QCOMPARE(cat.categoryItems(G::FocalLengthColumn).value("400"), 1);

    /* The folder NAME, not its path. */
    const QString folderName = QFileInfo(tmp.path()).fileName();
    QCOMPARE(cat.categoryItems(G::FolderNameColumn).value(folderName), 2);

    QCOMPARE(cat.categoryItems(G::CameraModelColumn).value("NIKON Z 9"), 2);
    QCOMPARE(cat.categoryItems(G::LensColumn).value("NIKKOR Z 100-400mm"), 1);
}

void tst_catalog::categoryItemsAreEmptyForColumnsTheIndexCannotAnswer()
{
/*
    Duplicates is a comparison of what is LOADED and the search flag is the panel's own
    box; neither means anything across a library. An empty answer is what tells the Find
    dock to HIDE those categories rather than show them empty, which would read as "you
    have no duplicates" -- a claim the catalog is in no position to make.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("fc1.nef", {"Heron"})});

    QVERIFY(cat.categoryItems(G::CompareColumn).isEmpty());
    QVERIFY(cat.categoryItems(G::SearchColumn).isEmpty());
    /* And a real one is not empty, so the assertions above are about the column and not
       about an empty catalog. */
    QVERIFY(!cat.categoryItems(G::KeywordsAllColumn).isEmpty());
}

void tst_catalog::genericIncludeAndExcludeNarrowTheSearch()
{
/* The Find dock hands the SAME checked-item structure to either scope, so the query
    carries category items as a map keyed by datamodel column rather than a named field
    per category. Within a column the values are OR-ed, columns are AND-ed, and exclude is
    AND-NOT -- exactly what checking and Opt+clicking items in the tree means. */
    Catalog &cat = Catalog::instance();
    CatalogRow z9 = rowFor("g-z9.nef");
    z9.model = "NIKON Z 9";
    z9.lens = "NIKKOR Z 100-400mm";
    CatalogRow z8 = rowFor("g-z8.nef");
    z8.model = "NIKON Z 8";
    z8.lens = "NIKKOR Z 24-70mm";
    CatalogRow d850 = rowFor("g-d850.nef");
    d850.model = "NIKON D850";
    d850.lens = "NIKKOR Z 100-400mm";
    cat.commit({z9, z8, d850});

    CatalogQuery q;
    q.include.insert(G::CameraModelColumn, {"NIKON Z 9", "NIKON Z 8"});
    QCOMPARE(cat.search(q).size(), 2);                  // OR within the column

    q.include.insert(G::LensColumn, {"NIKKOR Z 100-400mm"});
    QCOMPARE(cat.search(q), QStringList{imagePath("g-z9.nef")});   // AND between columns

    CatalogQuery x;
    x.exclude.insert(G::CameraModelColumn, {"NIKON D850"});
    QCOMPARE(cat.search(x).size(), 2);                  // AND-NOT

    /* An exclusion of something nothing carries subtracts nothing -- it must not be
       mistaken for an empty inclusion. */
    CatalogQuery none;
    none.exclude.insert(G::CameraModelColumn, {"CANON R5"});
    QCOMPARE(cat.search(none).size(), 3);
}

void tst_catalog::blankCategoryItemAccountsForEveryImage()
{
/*
    A single-valued category has to add up to the catalog. If 3 images are indexed and one
    has a lens, the Lens category says one lens and two blank -- not one lens and a silent
    shortfall the user has no way to read. This is how the Folders scope has always
    behaved: BuildFilters counts the empty string like any other key, so the panel already
    shows a blank first row there.

    AND THE BLANK MUST BE SELECTABLE, because a count nobody can click on is trivia.
    Checking it means "the ones with nothing here", which is why categorySql folds NULL
    into '' -- a plain column compare would drop the NULL rows out of both the list and
    the query. */
    Catalog &cat = Catalog::instance();
    CatalogRow withLens = rowFor("h-lens.nef");
    withLens.lens = "NIKKOR Z 100-400mm";
    withLens.captured = QDateTime(QDate(2024, 6, 15), QTime(9, 30), QTimeZone::utc());
    /* No lens, and no capture date at all -- captured lands in the database as NULL. */
    CatalogRow bare1 = rowFor("h-bare1.nef");
    bare1.captured = QDateTime();
    CatalogRow bare2 = rowFor("h-bare2.jpg");
    bare2.captured = QDateTime();
    cat.commit({withLens, bare1, bare2});

    int total = 0;
    cat.search(CatalogQuery(), -1, &total);
    QCOMPARE(total, 3);

    const QMap<QString, int> lenses = cat.categoryItems(G::LensColumn);
    QCOMPARE(lenses.value("NIKKOR Z 100-400mm"), 1);
    QCOMPARE(lenses.value(""), 2);
    int sum = 0;
    for (int n : lenses) sum += n;
    QCOMPARE(sum, total);

    /* A NULL date is blank, not 1970: IFNULL has to sit outside strftime. */
    const QMap<QString, int> years = cat.categoryItems(G::YearColumn);
    QCOMPARE(years.value("2024"), 1);
    QCOMPARE(years.value(""), 2);
    QVERIFY(!years.contains("1970"));

    /* Checking the blank row returns exactly the images with no lens. */
    CatalogQuery blank;
    blank.include.insert(G::LensColumn, {QString()});
    QStringList hits = cat.search(blank);
    hits.sort();
    QCOMPARE(hits, QStringList({imagePath("h-bare1.nef"), imagePath("h-bare2.jpg")}));

    /* And excluding it leaves the ones that have a value. */
    CatalogQuery notBlank;
    notBlank.exclude.insert(G::LensColumn, {QString()});
    QCOMPARE(notBlank.exclude.size(), 1);
    QCOMPARE(cat.search(notBlank), QStringList{imagePath("h-lens.nef")});
}

QTEST_MAIN(tst_catalog)
#include "tst_catalog.moc"
