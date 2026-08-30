#include <QtTest>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Cache/devpreviewcache.h"

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

    void schemaIsVersionThree();
    void commitThenSearchByKeyword();
    void hierarchyReachesAncestors();
    void freeTextFindsTitleAndGear();
    void unchangedRowsAreSkipped();
    void sidecarEditForcesReindex();
    void pathSpellingDoesNotSplitTheRow();
    void keywordFacetCountsImages();
    void staleOfReportsOnlyWhatChanged();
    void sweepDemotesMissingSource();
    void onMovedFollowsTheImage();
    void onDeletedRemovesTheRow();
    void searchTextIsNotInjectable();
    void clearingPreviewsLeavesTheCatalog();
    void staleOfSkipsUnchangedOnRescan();
    void removedKeywordDisappearsFromFacets();

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
    r.keywords = keywords;
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

void tst_catalog::schemaIsVersionThree()
{
    QCOMPARE(CacheDb::schemaVersion(), 3);
    QVERIFY(Catalog::instance().isAvailable());

    /* The catalog's tables were ADDED to the preview index's database, so both tenants
       must be usable in the same file at the same version. */
    QSqlDatabase db = CacheDb::instance().db();
    QVERIFY(db.isOpen());
    const auto tables = db.tables();
    for (const char *t : {"devpreview", "image", "keyword", "image_keyword", "image_fts"})
        QVERIFY2(tables.contains(t), t);
}

void tst_catalog::commitThenSearchByKeyword()
{
    Catalog &cat = Catalog::instance();
    QCOMPARE(cat.commit({rowFor("a.nef", {"Heron", "BC"})}), 1);
    QCOMPARE(cat.count(), 1);

    CatalogQuery q;
    q.keyword = "Heron";
    QCOMPARE(cat.search(q), QStringList{imagePath("a.nef")});

    /* Case must not matter: the user picks "heron" out of a facet list that may have been
       written by an application that capitalised it differently. */
    q.keyword = "hErOn";
    QCOMPARE(cat.search(q).size(), 1);

    q.keyword = "Eagle";
    QVERIFY(cat.search(q).isEmpty());
}

void tst_catalog::hierarchyReachesAncestors()
{
/*
    The reason hierarchical keywords are stored at all. The image is tagged only with the
    leaf "Heron" in dc:subject; the ANCESTORS ("Fauna", "Fauna|Bird") exist nowhere but
    lr:hierarchicalSubject. A search for a parent keyword must still find the picture.
*/
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("b.nef", {"Heron"}, {"Fauna|Bird|Heron"})});

    CatalogQuery q;
    for (const QString &k : {"Fauna", "Bird", "Heron", "Fauna|Bird"}) {
        q.keyword = k;
        QVERIFY2(cat.search(q).size() == 1,
                 qPrintable("ancestor search failed for " + k));
    }

    /* Free text must agree with the facet, or the two halves of the UI would disagree
       about the same picture. */
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
    q.keyword = "Eagle";
    QCOMPARE(cat.search(q).size(), 1);
    /* The OLD keyword must be gone, not merely outranked -- writeKeywords replaces the
       links rather than adding to them, which is how a keyword deleted in Lightroom
       disappears here too. */
    q.keyword = "Heron";
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

void tst_catalog::keywordFacetCountsImages()
{
    Catalog &cat = Catalog::instance();
    cat.commit({rowFor("g1.nef", {"Heron"}),
                rowFor("g2.nef", {"Heron"}),
                rowFor("g3.nef", {"Eagle"})});

    int heron = -1, eagle = -1;
    for (const CatalogKeyword &k : cat.keywords()) {
        if (k.path.isEmpty() && k.name == "Heron") heron = k.count;
        if (k.path.isEmpty() && k.name == "Eagle") eagle = k.count;
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

void tst_catalog::sweepDemotesMissingSource()
{
    Catalog &cat = Catalog::instance();
    const CatalogRow r = rowFor("i.nef", {"Heron"});
    cat.commit({r});

    CatalogQuery q;
    q.keyword = "Heron";
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

void tst_catalog::onMovedFollowsTheImage()
{
    Catalog &cat = Catalog::instance();
    const CatalogRow r = rowFor("j.nef", {"Heron"});
    cat.commit({r});

    const QString dst = imagePath("j-renamed.nef");
    QVERIFY(QFile::rename(r.path, dst));
    cat.onMoved(r.path, dst);

    CatalogQuery q;
    q.keyword = "Heron";
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
    ok.keyword = "Heron";
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

void tst_catalog::removedKeywordDisappearsFromFacets()
{
/*
    A keyword deleted in Lightroom must stop being offered here. The keyword VOCABULARY
    row survives (another image may still use it), but this image's link is replaced, so
    the facet count drops and a search stops returning it.
*/
    Catalog &cat = Catalog::instance();
    CatalogRow a = rowFor("fa.nef", {"Heron", "BC"});
    CatalogRow b = rowFor("fb.nef", {"Heron"});
    cat.commit({a, b});

    auto countOf = [&cat](const QString &name) {
        for (const CatalogKeyword &k : cat.keywords())
            if (k.path.isEmpty() && k.name == name) return k.count;
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
    q.keyword = "BC";
    QVERIFY(cat.search(q).isEmpty());
}

QTEST_MAIN(tst_catalog)
#include "tst_catalog.moc"
