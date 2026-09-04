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
    void outOfDateIgnoresWhatWasNeverIndexed();
    void availabilityLabelAndCodeAreOneSpelling();
    void fetchFreshReturnsWhatNeedNotBeRead();
    void searchRowsReturnsWhatFetchFreshWouldHave();
    void searchRowsMatchesSearchExactly();
    void fetchFreshAndStaleOfAgreeExactly();
    void displayFieldsSurviveTheRoundTrip();
    void sweepDemotesMissingSource();
    void availabilityDistinguishesOfflineFromMissing();
    void anEditedRowIsRewrittenNotJustMarkedStale();
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
    /* Declared ahead of the migration cases deliberately: those reopen the
       database from a fixture and a failure there leaves it shut for everything
       that follows, so a test placed after them cannot report its own result. */
    void folderCountsReportEveryFolderAndItsSize();
    void forgetFoldersDeletesExactlyThoseFolders();
    void forgettingEveryFolderKeepsTheKeywordVocabulary();
    void forgetFoldersHandlesMoreFoldersThanOneBind();
    void migrationFromVersionThreeMergesKeywords();
    void migrationCollapsesDoubledPathSeparators();
    void unreadableFileIsCataloguedAsAStubAndClearsWhenItParses();
    void categoryItemsMatchWhatTheDatamodelWrites();
    void categoryItemsAreEmptyForColumnsTheIndexCannotAnswer();
    void genericIncludeAndExcludeNarrowTheSearch();
    void blankCategoryItemAccountsForEveryImage();

private:
    QString imagePath(const QString &name) const;
    /* The same row, in a SUBFOLDER of the sandbox -- what the scope reconcile is about is
       which folder a row is in, so these tests need more than one. */
    CatalogRow rowIn(const QString &subFolder, const QString &name,
                     const QStringList &keywords = {});
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

CatalogRow tst_catalog::rowIn(const QString &subFolder, const QString &name,
                              const QStringList &keywords)
{
    QDir(tmp.path()).mkpath(subFolder);
    return rowFor(subFolder + "/" + name, keywords);
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
        keyword paths, which the flattening had made unrecoverable; version 8
        added no table at all -- it is a DATA REPAIR, collapsing the doubled
        path separator a trailing-slash catalog root wrote into image.path and
        devpreview.path; version 9 added image.unreadable, the flag that lets a
        file Winnow cannot parse be catalogued as a stub and reported as an
        Availability rather than vanishing. */
    QCOMPARE(CacheDb::schemaVersion(), 9);
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

void tst_catalog::outOfDateIgnoresWhatWasNeverIndexed()
{
    /*  THE ONE DIFFERENCE FROM staleOf, AND IT IS THE WHOLE REASON THE SECOND
        FUNCTION EXISTS.

        staleOf's caller is an INDEXER: it is about to read every file it is told
        about, and an image the catalog has never seen is exactly one of the files
        that must be read, so "unknown" is correctly reported as stale.

        outOfDate's caller is the scroll-in verification pass, which holds rows that
        were FILLED FROM the index and is asking which of them the index can no
        longer vouch for. A path the catalog does not know was never filled from it,
        so there is nothing to be stale against -- and reporting it would make the
        pass clear and re-read every uncatalogued row in the visible window, on every
        scroll, forever. */
    Catalog &cat = Catalog::instance();
    const CatalogRow a = rowFor("ood1.nef");
    CatalogRow b = rowFor("ood2.nef");
    cat.commit({a, b});

    QVERIFY(cat.outOfDate({a, b}).isEmpty());

    /*  A SIDECAR REWRITTEN BY SOMETHING ELSE is the case this exists for -- editing
        keywords in Lightroom moves the .xmp and never touches the raw -- so the
        stamp that must catch it is the sidecar's, not the image's. */
    b.sidecarMtime += 60;
    const auto stale = cat.outOfDate({a, b});
    QCOMPARE(stale.size(), 1);
    QVERIFY(stale.contains(b.path));

    /*  NEVER INDEXED: staleOf says stale, outOfDate says nothing. Both are right for
        their own caller, and the pair is asserted together so neither can quietly
        adopt the other's answer. */
    const CatalogRow unknown = rowFor("ood3.nef");
    QVERIFY(cat.staleOf({unknown}).contains(unknown.path));
    QVERIFY2(cat.outOfDate({unknown}).isEmpty(),
             "an unindexed path was never served from the index and cannot be out of date");
}

void tst_catalog::availabilityLabelAndCodeAreOneSpelling()
{
    /*  THE FILTER MAKES THIS LOAD-BEARING. A checked Filters item is compared
        against what the MODEL holds at Qt::EditRole, which for the Availability
        column is the int; the item shows the word. So the category item carries
        availabilityCode(label) and nothing matches unless the two are exact
        inverses. */
    for (int code : {int(Catalog::Availability::Present),
                     int(Catalog::Availability::Offline),
                     int(Catalog::Availability::Missing)}) {
        QCOMPARE(Catalog::availabilityCode(Catalog::availabilityLabel(code)), code);
    }

    /*  ANYTHING UNRECOGNISED IS PRESENT, which is the same answer an unset cell
        gives -- "a row nobody has asked about is openable until something says
        otherwise". */
    QCOMPARE(Catalog::availabilityCode("Whatever"), int(Catalog::Availability::Present));
    QCOMPARE(Catalog::availabilityCode(QString()), int(Catalog::Availability::Present));
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

void tst_catalog::searchRowsReturnsWhatFetchFreshWouldHave()
{
    /*  THE SAME ROW, FOUND THE OTHER WAY. searchRows exists so a catalog result can be
        loaded without a lookup per path -- but only if the row it returns is the row
        fetchFresh would have returned. A faster path that quietly drops a field would
        show the user a picture with no title, no rating or the wrong orientation, and
        nothing downstream could tell which query had filled it. */
    Catalog &cat = Catalog::instance();

    CatalogRow a = rowFor("rows1.jpg", {"Heron"}, {"Wildlife|Birds|Heron"});
    a.title = "Great Blue";
    a.creator = "R Hill";
    a.copyright = "(c) 2026";
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
    a.pick = true;
    a.gpsCoord = "49.28,-123.12";
    a.orientation = 6;
    a.exposureComp = "-0.3";
    a.developed = true;
    a.devPreviewKey = "abc123";
    a.shootingInfo = "1/1250 sec at f/5.6, ISO 800";
    a.captured = QDateTime::fromSecsSinceEpoch(1600000000);
    QCOMPARE(cat.commit({a}), 1);

    const QHash<QString, CatalogRow> viaFetch = cat.fetchFresh({a});
    QCOMPARE(viaFetch.size(), 1);

    CatalogQuery q;
    const QVector<CatalogRow> viaRows = cat.searchRows(q);
    QCOMPARE(viaRows.size(), 1);

    const CatalogRow &f = viaFetch.value(a.path);
    const CatalogRow &r = viaRows.first();

    QCOMPARE(r.path, f.path);
    QCOMPARE(r.folder, f.folder);
    QCOMPARE(r.filename, f.filename);
    QCOMPARE(r.ext, f.ext);
    QCOMPARE(r.captured, f.captured);
    QCOMPARE(r.rating, f.rating);
    QCOMPARE(r.label, f.label);
    QCOMPARE(r.pick, f.pick);
    QCOMPARE(r.title, f.title);
    QCOMPARE(r.creator, f.creator);
    QCOMPARE(r.copyright, f.copyright);
    QCOMPARE(r.make, f.make);
    QCOMPARE(r.model, f.model);
    QCOMPARE(r.lens, f.lens);
    QCOMPARE(r.iso, f.iso);
    QCOMPARE(r.aperture, f.aperture);
    QCOMPARE(r.shutter, f.shutter);
    QCOMPARE(r.focalLength, f.focalLength);
    QCOMPARE(r.width, f.width);
    QCOMPARE(r.height, f.height);
    QCOMPARE(r.gpsCoord, f.gpsCoord);
    QCOMPARE(r.orientation, f.orientation);
    QCOMPARE(r.exposureComp, f.exposureComp);
    QCOMPARE(r.developed, f.developed);
    QCOMPARE(r.devPreviewKey, f.devPreviewKey);
    QCOMPARE(r.shootingInfo, f.shootingInfo);
    QCOMPARE(r.keywordPaths, f.keywordPaths);

    /*  Keyword ORDER is not the contract -- fetchFresh reads one image's keywords at a
        time and searchRows reads them all in one join -- but the SET is. */
    QList<QString> rk = r.keywords, fk = f.keywords;
    rk.sort(); fk.sort();
    QCOMPARE(rk, fk);
    QVERIFY2(rk.contains("Wildlife"), "an ancestor did not come back");

    /*  IT DOES NOT ASK WHETHER THE FILE IS FRESH, which is the deliberate difference.
        A row whose file has changed is absent from fetchFresh and still present here:
        browsing shows it, and the stamp is checked when it is actually looked at. */
    CatalogRow changed = a;
    changed.srcMtime += 1;
    QVERIFY(cat.fetchFresh({changed}).isEmpty());
    QCOMPARE(cat.searchRows(q).size(), 1);
}

void tst_catalog::searchRowsMatchesSearchExactly()
{
    /*  ONE PREDICATE, TWO SHAPES. search() answers the count beside the results and
        searchRows() produces the rows themselves, so if they compiled the query
        differently the panel would say one number and show another set. They share
        buildQueryLocked precisely so that cannot drift, and this is what holds it. */
    Catalog &cat = Catalog::instance();

    CatalogRow heron = rowFor("sr-heron.jpg", {"Heron"});
    heron.model = "NIKON Z 9";
    heron.rating = 5;
    heron.captured = QDateTime::fromSecsSinceEpoch(1600000000);

    CatalogRow eagle = rowFor("sr-eagle.jpg", {"Eagle"});
    eagle.model = "SONY ILCE-9M2";
    eagle.rating = 2;
    eagle.captured = QDateTime::fromSecsSinceEpoch(1700000000);

    CatalogRow gull = rowFor("sr-gull.jpg", {"Gull"});
    gull.model = "NIKON Z 9";
    gull.rating = 1;
    gull.captured = QDateTime::fromSecsSinceEpoch(1500000000);

    QCOMPARE(cat.commit({heron, eagle, gull}), 3);

    const auto pathsOf = [](const QVector<CatalogRow> &rows) {
        QStringList out;
        for (const CatalogRow &r : rows) out << r.path;
        return out;
    };

    /*  Every query shape the two must agree on: everything, a keyword, a column
        restriction, free text, and a rating floor. */
    QVector<CatalogQuery> queries;
    CatalogQuery all;                       queries << all;
    CatalogQuery byKeyword;                 byKeyword.keywords << "Heron";
    queries << byKeyword;
    CatalogQuery byModel;                   byModel.model = "NIKON Z 9";
    queries << byModel;
    CatalogQuery byText;                    byText.text = "Eagle";
    queries << byText;
    CatalogQuery byRating;                  byRating.minRating = 3;
    queries << byRating;

    for (const CatalogQuery &q : queries) {
        int searchTotal = 0;
        int rowsTotal = 0;
        const QStringList paths = cat.search(q, 0, &searchTotal);
        const QVector<CatalogRow> rows = cat.searchRows(q, 0, &rowsTotal);
        QCOMPARE(rowsTotal, searchTotal);
        /*  ORDER INCLUDED, not just membership: both order by captured DESC with id as
            the tie-break, and a grid that reshuffled when the rows arrived by the other
            path would be the visible symptom. */
        QCOMPARE(pathsOf(rows), paths);
    }

    /*  The limit applies the same way, and cuts the same end of the ordering. */
    int total = 0;
    const QVector<CatalogRow> capped = cat.searchRows(CatalogQuery(), 2, &total);
    QCOMPARE(capped.size(), 2);
    QCOMPARE(total, 3);
    QCOMPARE(pathsOf(capped), cat.search(CatalogQuery(), 2));
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

void tst_catalog::anEditedRowIsRewrittenNotJustMarkedStale()
{
    /*  A rating or colour edit writes the sidecar, which moves its mtime and so
        makes the catalog row STALE -- and in folder scope that is enough, because
        the next visit re-indexes it. In CATALOG scope the user may never open the
        containing folder again, so "stale" would mean "wrong, indefinitely": rate
        an image, search for that rating, and not find it.

        MW::updateCatalogForRow pushes the whole row back through the same builder
        the bulk capture uses. This pins the catalog half -- that a re-commit of an
        edited row REPLACES the stored values and restores freshness, rather than
        being skipped as unchanged. */
    Catalog &cat = Catalog::instance();

    CatalogRow r = rowFor("edited.jpg", {"Heron"});
    r.rating = 0;
    r.label = "";
    QCOMPARE(cat.commit({r}), 1);

    const auto before = cat.fetchFresh({r}).value(r.path);
    QCOMPARE(before.rating, 0);

    /*  The edit: the sidecar has been rewritten, so its stamp moves, and the row
        carries the new values. */
    CatalogRow edited = r;
    edited.rating = 4;
    edited.label = "Red";
    edited.sidecarMtime = r.sidecarMtime + 10;

    QCOMPARE(cat.commit({edited}), 1);

    /*  Fresh against the NEW stamp, and holding the new values -- so a search on
        rating finds it and nothing will re-read the file to discover that. */
    const auto after = cat.fetchFresh({edited});
    QVERIFY2(after.contains(edited.path), "the re-committed row is not fresh");
    QCOMPARE(after.value(edited.path).rating, 4);
    QCOMPARE(after.value(edited.path).label, QString("Red"));

    /*  And the OLD stamp no longer matches, which is what stops a stale reader
        from serving the pre-edit values. */
    QVERIFY2(!cat.fetchFresh({r}).contains(r.path),
             "the pre-edit stamp must no longer read as fresh");

    /*  Committing the same row again is skipped, so the write-back costs nothing
        when the catalog is already right -- it is called on every edit. */
    QCOMPARE(cat.commit({edited}), 0);
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
        /*  THE FIXTURE MUST BE A FAITHFUL VERSION 3 FILE, devpreview included. Every
            real one has it: schema 1 creates the table and schema 2 adds its pathkey, so
            a file cannot reach version 3 without going through both. Omitting it here
            made the fixture a database that has never existed in the field -- and schema
            8's data repair, which touches devpreview, failed on it. A failed migration
            is MOVED ASIDE and recreated, so this test then ran against a fresh empty
            index whose user_version was legitimately current: the version assertion
            passed, the keywords were simply gone, and the reported failure ("0 keywords,
            expected 3") pointed at the merge rather than at the fixture. It also left
            CacheDb pointing at a recreated file, which is why every case after this one
            failed too. */
        QVERIFY(q.exec("CREATE TABLE devpreview (id INTEGER PRIMARY KEY, path TEXT NOT"
                       " NULL, folder TEXT NOT NULL, hash TEXT NOT NULL, bytes INTEGER"
                       " NOT NULL, used INTEGER NOT NULL, live INTEGER NOT NULL DEFAULT"
                       " 1, vol TEXT NOT NULL DEFAULT '', srcsize INTEGER NOT NULL"
                       " DEFAULT 0, srcmtime INTEGER NOT NULL DEFAULT 0,"
                       " pathkey TEXT NOT NULL DEFAULT '')"));
        QVERIFY(q.exec("CREATE UNIQUE INDEX devpreview_pathkey ON devpreview(pathkey)"));
        QVERIFY(q.exec("CREATE INDEX devpreview_evict ON devpreview(live, used)"));
        QVERIFY(q.exec("CREATE INDEX devpreview_folder ON devpreview(folder)"));
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


void tst_catalog::unreadableFileIsCataloguedAsAStubAndClearsWhenItParses()
{
/*
    A FILE THE PARSERS CANNOT READ MUST STILL BE ACCOUNTABLE FOR. Dropped, it is on disk,
    absent from the index, and the only evidence is a gap between two counts that no scan
    can close -- which is exactly how it presented in the field. Recorded as a stub, it is
    one Availability value the user can filter on.

    AND THE FLAG MUST CLEAR. A file that is replaced, repaired, or whose format Winnow
    later learns, parses on the next scan; if the ordinary commit did not clear the flag,
    the row would go on claiming to be unreadable forever.
*/
    Catalog &cat = Catalog::instance();
    const QString path = imagePath("a.nef");

    CatalogRow stub;
    stub.path = path;
    stub.folder = QFileInfo(path).absoluteDir().path();
    stub.filename = QFileInfo(path).fileName();
    stub.ext = "nef";
    stub.srcSize = QFileInfo(path).size();
    stub.srcMtime = QFileInfo(path).lastModified().toSecsSinceEpoch();

    QCOMPARE(cat.commitUnreadable({stub}), 1);
    QCOMPARE(cat.unreadableCount(), 1);
    QCOMPARE(cat.count(), 1);           // it IS catalogued, which is the point

    auto avail = cat.availabilityOf({path});
    QVERIFY(avail.contains(path));
    QCOMPARE(int(avail.value(path)), int(Catalog::Availability::Unreadable));

    /* The same file, parsed. One ordinary commit and the row is a normal row again. */
    QCOMPARE(cat.commit({rowFor("a.nef", {"Heron"})}), 1);
    QCOMPARE(cat.unreadableCount(), 0);
    QCOMPARE(cat.count(), 1);           // updated in place, not duplicated
    avail = cat.availabilityOf({path});
    QCOMPARE(int(avail.value(path)), int(Catalog::Availability::Present));
}

void tst_catalog::migrationCollapsesDoubledPathSeparators()
{
/*
    SCHEMA 8 IS A DATA REPAIR, and the rows it repairs are the ones a shipped build wrote
    -- a scanner root spelled with a trailing separator put a doubled slash into every
    path below it (5,059 rows of 6,200 on the library that found it). A fresh database
    proves nothing about that, so this puts a current file BACK to version 7 with the
    damaged rows in it, the way tst_thumbcache exercises its own migration.

    THE COLLIDING ROW IS THE INTERESTING ONE. Rows whose cleaned path already exists must
    be deleted rather than rewritten, and their FTS row with them -- image_fts has no
    foreign key onto image, so a row left behind there would keep matching searches for an
    image that no longer exists.
*/
    /*  SCOPED, so no QSqlDatabase or QSqlQuery of ours is still alive when the
        connection is closed below. Held open across closeThisThread they keep the
        connection in use, Qt refuses to retire it ("connection ... is still in use"),
        and the reopen -- which addDatabase's the SAME name, the generation being
        unchanged -- comes back CLOSED. The failure then reads "the version 7 index
        failed to reopen", which sounds like the migration and is not. */
    {
        QSqlDatabase db = CacheDb::instance().db();
        QVERIFY(db.isOpen());
        QSqlQuery q(db);

        /* pathkey is spelled RAW here, not cleaned, purely so the two collision rows can
           coexist under a unique index; the field itself is not what schema 8 repairs. */
        QVERIFY(q.exec("INSERT INTO image (id, pathkey, path, folder) VALUES"
                       " (901, '/lib//solo.nef', '/lib//solo.nef', '/lib'),"
                       " (902, '/lib/twin.nef',  '/lib/twin.nef',  '/lib'),"
                       " (903, '/lib//twin.nef', '/lib//twin.nef', '/lib')"));
        QVERIFY(q.exec("INSERT INTO image_fts (rowid, filename) VALUES"
                       " (901, 'solo.nef'), (902, 'twin.nef'), (903, 'twin.nef')"));
        QVERIFY(q.exec("INSERT INTO devpreview (id, path, folder, hash, bytes, used)"
                       " VALUES (901, '/lib//solo.jpg', '/lib', 'h', 1, 1)"));
        /*  PUTTING THE FILE BACK TO 7 MEANS UNDOING WHAT CAME AFTER 7, not just
            rewriting the number. The sandbox file was created at the current schema, so
            it already carries schema 9's `unreadable` column; stamping user_version to 7
            and reopening ran v9's ALTER TABLE ... ADD COLUMN against a table that
            already had it, which fails -- and a failed migration is moved aside and
            recreated, so this case then asserted against a fresh EMPTY index and the
            repair it exists to prove was never executed at all. It reported as "the
            version 7 index failed to reopen", which named the symptom and not the cause.

            THIS IS A MAINTENANCE POINT: every schema added above 7 has to be undone
            here, or this test silently stops testing anything. */
        QVERIFY(q.exec("ALTER TABLE image DROP COLUMN unreadable"));
        QVERIFY(q.exec("PRAGMA user_version = 7"));
    }

    CacheDb::instance().closeThisThread();
    QSqlDatabase back = CacheDb::instance().db();
    QVERIFY2(back.isOpen(), "the version 7 index failed to reopen");

    QSqlQuery v(back);
    QVERIFY(v.exec("PRAGMA user_version") && v.next());
    QCOMPARE(v.value(0).toInt(), CacheDb::schemaVersion());

    /* Nothing anywhere still carries the doubled separator. */
    for (const char *sql : {"SELECT COUNT(*) FROM image WHERE path LIKE '%//%'",
                            "SELECT COUNT(*) FROM image WHERE folder LIKE '%//%'",
                            "SELECT COUNT(*) FROM devpreview WHERE path LIKE '%//%'"}) {
        QVERIFY(v.exec(QString::fromLatin1(sql)) && v.next());
        QCOMPARE(v.value(0).toInt(), 0);
    }

    /* The row with no twin was rewritten, keeping its id and its FTS row. */
    QVERIFY(v.exec("SELECT path FROM image WHERE id = 901") && v.next());
    QCOMPARE(v.value(0).toString(), QString("/lib/solo.nef"));
    QVERIFY(v.exec("SELECT COUNT(*) FROM image_fts WHERE rowid = 901") && v.next());
    QCOMPARE(v.value(0).toInt(), 1);

    /* The duplicate went, the original stayed, and no FTS row was orphaned. */
    QVERIFY(v.exec("SELECT COUNT(*) FROM image WHERE id = 903") && v.next());
    QCOMPARE(v.value(0).toInt(), 0);
    QVERIFY(v.exec("SELECT COUNT(*) FROM image WHERE id = 902") && v.next());
    QCOMPARE(v.value(0).toInt(), 1);
    QVERIFY(v.exec("SELECT COUNT(*) FROM image_fts WHERE rowid = 903") && v.next());
    QCOMPARE(v.value(0).toInt(), 0);

    QVERIFY(v.exec("SELECT path FROM devpreview WHERE id = 901") && v.next());
    QCOMPARE(v.value(0).toString(), QString("/lib/solo.jpg"));
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
    a.iso = 800;
    a.captured = QDateTime(QDate(2024, 6, 15), QTime(9, 30), QTimeZone::utc());
    CatalogRow b = rowFor("fb1.jpg");
    b.model = "NIKON Z 9";
    b.rating = 0;
    b.pick = false;
    b.iso = 6400;
    b.captured = QDateTime(QDate(2024, 6, 15), QTime(10, 0), QTimeZone::utc());
    cat.commit({a, b});

    /* Type is the suffix UPPER-cased, as DataModel::addFileDataForRow writes it. */
    const QMap<QString, int> types = cat.categoryItems(G::TypeColumn);
    QCOMPARE(types.value("NEF"), 1);
    QCOMPARE(types.value("JPG"), 1);

    /* Year "yyyy" and Day "yyyy-MM-dd", as addMetadataForItem writes them. */
    QCOMPARE(cat.categoryItems(G::YearColumn).value("2024"), 2);
    QCOMPARE(cat.categoryItems(G::DayColumn).value("2024-06-15"), 2);

    /* Month is the English abbreviation Catalog::monthLabel spells, NOT a locale's
       month name and not "06": the datamodel writes that string into G::MonthColumn,
       so the SQL CASE has to produce the same one or the same June would be two
       different filter items in the two scopes. */
    QCOMPARE(cat.categoryItems(G::MonthColumn).value("Jun"), 2);
    QCOMPARE(cat.categoryItems(G::MonthColumn).value(Catalog::monthLabel(6)), 2);

    /* ISO is right-justified to six, which is what BuildFilters does to the datamodel's
       int before counting it -- unpadded, "800" would sort between "100" and "8000". */
    const QMap<QString, int> isos = cat.categoryItems(G::ISOColumn);
    QCOMPARE(isos.value("   800"), 1);
    QCOMPARE(isos.value("  6400"), 1);
    QVERIFY(!isos.contains("800"));

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

void tst_catalog::folderCountsReportEveryFolderAndItsSize()
{
/*
    One query has to answer both halves of a scope reconcile: WHICH folders are
    catalogued, so each can be put to the scope table, and what forgetting one would
    cost, so the total can be shown before anything is deleted. Asking countUnder per
    folder would be the same answer for one query per folder.
*/
    Catalog &cat = Catalog::instance();
    const QString a = QDir(tmp.path()).absoluteFilePath("fc-a");
    const QString b = QDir(tmp.path()).absoluteFilePath("fc-b");
    cat.commit({rowIn("fc-a", "one.jpg"), rowIn("fc-a", "two.jpg"),
                rowIn("fc-b", "three.jpg")});

    const QMap<QString, int> counts = cat.folderCounts();
    QCOMPARE(counts.size(), 2);
    QCOMPARE(counts.value(a), 2);
    QCOMPARE(counts.value(b), 1);

    /* The sum is the catalog, which is what makes "this empties it" a safe thing to say
       in the confirmation. */
    int sum = 0;
    for (int n : counts) sum += n;
    QCOMPARE(sum, cat.count());
}

void tst_catalog::forgetFoldersDeletesExactlyThoseFolders()
{
/*
    Exact folders, not a prefix. The reconcile has already decided folder by folder --
    an included branch may sit inside an excluded tree -- so a prefix delete would take
    rows the scope still admits.

    AND THE FTS ROWS MUST GO WITH THEM. image_fts has no foreign key onto image, so a
    delete that forgot it would leave the images unfindable by path and still matching a
    keyword search, which is the worst of both.
*/
    Catalog &cat = Catalog::instance();
    const QString keep = QDir(tmp.path()).absoluteFilePath("ff-keep");
    const QString drop = QDir(tmp.path()).absoluteFilePath("ff-drop");
    const QString nested = QDir(tmp.path()).absoluteFilePath("ff-drop/inner");
    cat.commit({rowIn("ff-keep", "keeper.jpg", {"heron"}),
                rowIn("ff-drop", "goner.jpg", {"osprey"}),
                rowIn("ff-drop/inner", "deeper.jpg", {"osprey"})});
    QCOMPARE(cat.count(), 3);

    /* Only the named folder: the nested one was not asked for and must survive, which is
       exactly what a prefix delete would get wrong. */
    QCOMPARE(cat.forgetFolders({drop}), 1);
    QCOMPARE(cat.count(), 2);

    const QMap<QString, int> after = cat.folderCounts();
    QCOMPARE(after.value(keep), 1);
    QCOMPARE(after.value(nested), 1);
    QVERIFY(!after.contains(drop));

    /* The free-text row went too -- "osprey" now finds only the nested image. */
    CatalogQuery q;
    q.text = "osprey";
    QCOMPARE(cat.search(q), QStringList{imagePath("ff-drop/inner/deeper.jpg")});

    QCOMPARE(cat.forgetFolders({nested}), 1);
    q.text = "osprey";
    QVERIFY(cat.search(q).isEmpty());
    q.text = "heron";
    QCOMPARE(cat.search(q), QStringList{imagePath("ff-keep/keeper.jpg")});
}

void tst_catalog::forgettingEveryFolderKeepsTheKeywordVocabulary()
{
/*
    THE VOCABULARY IS USER INTENT, NOT AN INDEX. Emptying the scope table empties the
    catalog -- every image row, every FTS row, every image_keyword link -- but the
    keywords the user has accumulated are not something a scan rebuilds from files it may
    no longer be pointed at, and a keywords module will own them. This is why the whole
    catalog path goes through forgetFolders and NOT through clear(), which does delete
    the vocabulary and keeps that meaning for callers who want it.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowIn("kv-a", "one.jpg", {"heron"}),
                rowIn("kv-b", "two.jpg", {"osprey"})});
    QCOMPARE(cat.keywords().size(), 2);

    QStringList all;
    const QMap<QString, int> counts = cat.folderCounts();
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) all << it.key();
    QCOMPARE(cat.forgetFolders(all), 2);

    QCOMPARE(cat.count(), 0);
    QVERIFY(cat.folderCounts().isEmpty());

    /* Still two keywords, now attached to nothing -- a count of 0, not an absence. */
    const QList<CatalogKeyword> kws = cat.keywords();
    QCOMPARE(kws.size(), 2);
    for (const CatalogKeyword &k : kws) QCOMPARE(k.count, 0);

    /* clear() is the other statement, and it still means what it always did. */
    cat.clear();
    QVERIFY(cat.keywords().isEmpty());
}

void tst_catalog::forgetFoldersHandlesMoreFoldersThanOneBind()
{
/*
    A library holds more folders than SQLite will take bound variables, so the IN list is
    chunked -- inside ONE transaction, because a reconcile that half succeeded would
    leave the catalog disagreeing with the scope table it was just made to match.
*/
    Catalog &cat = Catalog::instance();
    QVector<CatalogRow> rows;
    const int n = 1200;                 // more than two chunks
    for (int i = 0; i < n; ++i)
        rows << rowIn(QString("fb-%1").arg(i, 4, 10, QChar('0')), "img.jpg");
    cat.commit(rows);
    QCOMPARE(cat.count(), n);

    QStringList folders;
    const QMap<QString, int> counts = cat.folderCounts();
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        folders << it.key();
    QCOMPARE(folders.size(), n);

    /* All but the last, so a chunk boundary is not the same thing as the end. */
    const QString survivor = folders.takeLast();
    QCOMPARE(cat.forgetFolders(folders), n - 1);
    QCOMPARE(cat.count(), 1);
    QCOMPARE(cat.folderCounts().value(survivor), 1);
}

QTEST_MAIN(tst_catalog)
#include "tst_catalog.moc"
