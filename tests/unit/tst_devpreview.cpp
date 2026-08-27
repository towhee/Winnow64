#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "Cache/cachedb.h"
#include "Cache/devpreviewcache.h"

/*
    DevPreviewCache -- the on-disk cache of screen-resolution develop previews.

    The behaviours worth pinning are the ones that protect the user's data or their
    patience, not the plumbing:

      o a preview is never served for a recipe it was not rendered from
      o a rename costs an index edit, not a file rewrite
      o the orphan sweep NEVER acts on an unmounted volume (an ejected card must not
        look like a folder full of deleted images)
      o the byte cap actually bounds the cache, evicting demoted entries first
      o a path reused by a DIFFERENT image never serves the previous occupant's preview,
        even when the two share a recipe (which a preset makes the normal case)
      o an existing JSON index is carried into the database rather than abandoned
      o the same image spelled two ways is ONE entry, not two
      o the payload is read without the cache mutex, so decoder threads run in parallel
*/
class tst_devpreview : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void putGetRoundTrip();
    void staleRecipeMisses();
    void movePreservesPreviewWithoutRewritingIt();
    void deleteRemovesEntryAndFile();
    void capEvictsAndPrefersDemoted();
    void lruEvictsLeastRecentlyUsedAtFullSize();
    void containsAgreesWithGet();
    void sweepDemotesMissingSource();
    void sweepSkipsUnmountedVolume();
    void indexSurvivesReload();
    void putCommitsIndexWithoutAnExplicitSave();
    void lazyLoadPreservesIdsAcrossSessions();
    void reconcileDropsStrayFiles();
    void reusedPathDoesNotServeThePreviousImage();
    void pathSpellingDoesNotSplitTheEntry();
    void concurrentGetsAreNotSerialised();
    void legacyJsonIndexIsImported();
    void isCachePathIdentifiesTheProtectedFolder();

private:
    QString imagePath(const QString &name) const;
    static QByteArray jpg(int fill, int kb = 4);

    QTemporaryDir tmp;          // stands in for the image folder
    QTemporaryDir cacheTmp;     // stands in for AppDataLocation/PreviewCache
};

QString tst_devpreview::imagePath(const QString &name) const
{
    return QDir(tmp.path()).absoluteFilePath(name);
}

/* A payload that is merely bytes -- the cache never decodes what it stores. */
QByteArray tst_devpreview::jpg(int fill, int kb)
{
    return QByteArray(kb * 1024, char(fill));
}

void tst_devpreview::initTestCase()
{
/*
    Keep every path this test touches inside the test sandbox. init() calls clear() before
    it has pointed the cache anywhere, which without this resolves to the real
    AppDataLocation -- and clear() deletes what it finds there.
*/
    QStandardPaths::setTestModeEnabled(true);
}

void tst_devpreview::cleanupTestCase()
{
    /* Release the database handle before QTemporaryDir removes the directory under it. */
    CacheDb::instance().closeThisThread();
}

void tst_devpreview::init()
{
    QVERIFY(tmp.isValid());
    QVERIFY(cacheTmp.isValid());
    DevPreviewCache &c = DevPreviewCache::instance();
    c.clear();
    c.setCacheDir(cacheTmp.path());
    c.setMaxBytes(20LL * 1024 * 1024 * 1024);
    c.clear();
}

void tst_devpreview::putGetRoundTrip()
{
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("a.nef");
    const QByteArray payload = jpg(0x41);

    c.put(p, "recipe1", payload);
    QVERIFY(c.contains(p, "recipe1"));
    QCOMPARE(c.get(p, "recipe1"), payload);
    QCOMPARE(c.count(), 1);
    QCOMPARE(c.totalBytes(), qint64(payload.size()));
}

void tst_devpreview::staleRecipeMisses()
{
/*
    The whole point of storing the recipe hash: after an edit (or an edit made by another
    machine and synced in) the cached pixels describe a picture the user has moved on
    from. Showing them would be worse than showing nothing.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("a.nef");

    c.put(p, "recipe1", jpg(0x41));
    QVERIFY(c.get(p, "recipe2").isEmpty());
    QVERIFY(!c.contains(p, "recipe2"));
    // the entry is still there for its own recipe -- a miss is not an eviction
    QVERIFY(!c.get(p, "recipe1").isEmpty());
}

void tst_devpreview::movePreservesPreviewWithoutRewritingIt()
{
/*
    Cache files are named by an opaque id precisely so a rename or a move is an index
    edit. If this ever regresses into "rename the file on disk", moving a folder of
    edited images becomes O(n) file operations for no benefit.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString src = imagePath("old.nef");
    const QString dst = imagePath("new.nef");
    const QByteArray payload = jpg(0x42);
    c.put(src, "recipe1", payload);

    QDir cacheDir(c.cacheDir());
    const QStringList before = cacheDir.entryList(QStringList() << "*.jpg", QDir::Files);
    QCOMPARE(before.size(), 1);

    c.onMoved(src, dst);

    QCOMPARE(c.get(dst, "recipe1"), payload);
    QVERIFY(c.get(src, "recipe1").isEmpty());
    QCOMPARE(c.count(), 1);

    // same file on disk, same name: nothing was rewritten
    const QStringList after = cacheDir.entryList(QStringList() << "*.jpg", QDir::Files);
    QCOMPARE(after, before);
}

void tst_devpreview::deleteRemovesEntryAndFile()
{
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("a.nef");
    c.put(p, "recipe1", jpg(0x43));
    QCOMPARE(c.count(), 1);

    c.onDeleted(p);

    QCOMPARE(c.count(), 0);
    QCOMPARE(c.totalBytes(), qint64(0));
    QDir cacheDir(c.cacheDir());
    QVERIFY(cacheDir.entryList(QStringList() << "*.jpg", QDir::Files).isEmpty());
}

void tst_devpreview::capEvictsAndPrefersDemoted()
{
/*
    The cap is the load-bearing bound -- the sweep only reclaims previews whose source is
    gone, while the ordinary case is thousands of images that all still exist. Demoted
    entries (source missing at the last sweep) must go first.
*/
    DevPreviewCache &c = DevPreviewCache::instance();

    // three real files so the sweep can demote exactly one of them
    QStringList paths;
    for (const QString &n : {QString("a.nef"), QString("b.nef"), QString("c.nef")}) {
        const QString p = imagePath(n);
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
        paths << p;
        c.put(p, "recipe1", jpg(0x44, 4));       // 4 KB each
    }
    QCOMPARE(c.count(), 3);

    // b's source disappears; the sweep demotes it
    QVERIFY(QFile::remove(paths.at(1)));
    QCOMPARE(c.sweep(), 1);

    // room for two entries -> the demoted one is the one that goes
    c.setMaxBytes(9 * 1024);

    QCOMPARE(c.count(), 2);
    QVERIFY(c.get(paths.at(1), "recipe1").isEmpty());   // b, demoted, evicted
    QVERIFY(!c.get(paths.at(0), "recipe1").isEmpty());
    QVERIFY(!c.get(paths.at(2), "recipe1").isEmpty());
    QVERIFY(c.totalBytes() <= 9 * 1024);
}

void tst_devpreview::lruEvictsLeastRecentlyUsedAtFullSize()
{
/*
    devPreviews are written at FULL SENSOR RESOLUTION by default, so an entry is several
    MB rather than a few hundred KB, and the cap is reached by ordinary use rather than
    only by a pathological folder. Among entries that all still exist -- the dominant
    case, which no sweep ever reclaims -- eviction must follow last use, so the image the
    user is working on survives and the ones they left behind go.

    NOTE the sleep. lastUsed has ONE-SECOND resolution, so entries written in the same
    second are tied and evictLocked falls back to hash order between them. That is
    harmless in use (a tie means they really were used together) but it means a test
    cannot assert an order without letting the clock move.
*/
    DevPreviewCache &c = DevPreviewCache::instance();

    QStringList paths;
    for (const QString &n : {QString("x.nef"), QString("y.nef"), QString("z.nef")}) {
        const QString p = imagePath(n);
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
        paths << p;
        /* 3 MB each: the order of magnitude a full-resolution devPreview actually is. */
        c.put(p, "recipe1", jpg(0x55, 3 * 1024));
    }
    QCOMPARE(c.count(), 3);

    QTest::qSleep(1100);
    QVERIFY(!c.get(paths.at(0), "recipe1").isEmpty());   // x is now the most recent

    // room for two of the three
    c.setMaxBytes(7LL * 1024 * 1024);

    QCOMPARE(c.count(), 2);
    QVERIFY(!c.get(paths.at(0), "recipe1").isEmpty());   // most recently used, kept
    QVERIFY(c.totalBytes() <= 7LL * 1024 * 1024);
}

void tst_devpreview::containsAgreesWithGet()
{
/*
    contains() is what decides whether an image needs a devPreview built
    (MW::devPreviewNeedsBuild) and whether topUpDevPreviews has work to do. If it could
    disagree with get() the builder would either skip images that have no usable preview
    or re-render ones that do -- silently, and for every image in the folder.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("agree.nef");

    QVERIFY(!c.contains(p, "recipe1"));
    QVERIFY(c.get(p, "recipe1").isEmpty());

    c.put(p, "recipe1", jpg(0x66, 8));
    QVERIFY(c.contains(p, "recipe1"));
    QVERIFY(!c.get(p, "recipe1").isEmpty());

    // a different recipe is a miss for both, not just for get()
    QVERIFY(!c.contains(p, "recipe2"));
    QVERIFY(c.get(p, "recipe2").isEmpty());
}

void tst_devpreview::sweepDemotesMissingSource()
{
/*
    Demote, never delete: a file that comes back from the trash finds its preview intact.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("gone.nef");
    QFile f(p);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();
    c.put(p, "recipe1", jpg(0x45));

    QCOMPARE(c.sweep(), 0);          // still there, nothing to do
    QVERIFY(QFile::remove(p));
    QCOMPARE(c.sweep(), 1);

    // demoted, NOT deleted -- the pixels are still served
    QCOMPARE(c.count(), 1);
    QVERIFY(!c.get(p, "recipe1").isEmpty());
}

void tst_devpreview::sweepSkipsUnmountedVolume()
{
/*
    THE important one. Winnow browses memory cards and external drives constantly. If the
    sweep treated "path does not exist" on an unmounted volume as a deletion, ejecting a
    drive and restarting would demote -- and then evict -- every preview on it.

    Simulated by writing an entry whose recorded mount point does not exist, which is
    exactly the state an ejected volume leaves behind.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("oncard.nef");
    c.put(p, "recipe1", jpg(0x46));
    c.save();

    // rewrite the row so the entry claims to live on a volume that is not mounted
    {
        QSqlDatabase db = CacheDb::instance().db();
        QVERIFY(db.isOpen());
        QSqlQuery q(db);
        QVERIFY(q.exec("UPDATE devpreview SET vol = '/Volumes/__WinnowNoSuchVolume__'"));
        QCOMPARE(q.numRowsAffected(), 1);
    }

    c.setCacheDir(QString());               // force a genuine reload below
    c.setCacheDir(cacheTmp.path());
    QCOMPARE(c.count(), 1);

    // the source does not exist (it never did) -- but the volume is not mounted
    QCOMPARE(c.sweep(), 0);
    QVERIFY(!c.get(p, "recipe1").isEmpty());
}

void tst_devpreview::indexSurvivesReload()
{
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("a.nef");
    const QByteArray payload = jpg(0x47);
    c.put(p, "recipe1", payload);
    c.save();

    c.setCacheDir(QString());               // point elsewhere, then back
    c.setCacheDir(cacheTmp.path());

    QCOMPARE(c.count(), 1);
    QCOMPARE(c.get(p, "recipe1"), payload);
}

void tst_devpreview::putCommitsIndexWithoutAnExplicitSave()
{
/*
    REGRESSION. The index was committed only by MW::closeEvent. Any abnormal exit --
    force quit, crash, power loss -- therefore did not merely FORGET the previews written
    that session: the next launch's reconcile() deleted the .jpg files the index did not
    name, while the 256px thumbnails written to the sidecars in the same flush survived.
    That left exactly the state this cache exists to prevent -- the grid showing the
    developed picture and the loupe showing the camera's, with no way back short of
    re-rendering the folder.

    put() now commits the index with the payload, so a session that never reaches
    closeEvent still leaves a usable cache.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("a.nef");
    const QByteArray payload = jpg(0x61);
    c.put(p, "recipe1", payload);
    // NO c.save() -- stands in for a session that never shut down cleanly

    c.setCacheDir(QString());               // a new session, same directory
    c.setCacheDir(cacheTmp.path());

    QCOMPARE(c.count(), 1);
    QCOMPARE(c.get(p, "recipe1"), payload);
}

void tst_devpreview::lazyLoadPreservesIdsAcrossSessions()
{
/*
    REGRESSION. The index is read lazily on first use, and that is the ONLY load path --
    nothing in the app calls load() or setCacheDir(). When the load was tied to an
    explicit init call instead, the app never made it, and the failure was silent and
    destructive: the cache started empty every session (so every prior-session preview
    missed), and nextId restarted at 1, so the first new preview OVERWROTE the cache file
    belonging to a completely different image. Observed on disk as a 1.jpg that had been
    clobbered and a 2.jpg orphaned out of the index.

    Simulating a new session: setCacheDir marks the cache unloaded without reading
    anything, so the first call below is what triggers the load.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString a = imagePath("a.nef");
    const QString b = imagePath("b.nef");
    c.put(a, "recipe1", jpg(0x51));
    c.put(b, "recipe1", jpg(0x52));
    c.save();

    QDir cacheDir(c.cacheDir());
    QCOMPARE(cacheDir.entryList(QStringList() << "*.jpg", QDir::Files).size(), 2);

    // a "new session": same directory, nothing loaded yet
    c.setCacheDir(QString());
    c.setCacheDir(cacheTmp.path());

    // first touch must see the prior session's entries...
    QCOMPARE(c.count(), 2);
    QCOMPARE(c.get(a, "recipe1"), jpg(0x51));

    // ...and a new preview must take a FRESH id, not clobber an existing file
    const QString d = imagePath("d.nef");
    c.put(d, "recipe1", jpg(0x53));
    QCOMPARE(cacheDir.entryList(QStringList() << "*.jpg", QDir::Files).size(), 3);
    QCOMPARE(c.get(a, "recipe1"), jpg(0x51));      // untouched
    QCOMPARE(c.get(b, "recipe1"), jpg(0x52));      // untouched
    QCOMPARE(c.get(d, "recipe1"), jpg(0x53));
}

void tst_devpreview::reconcileDropsStrayFiles()
{
/*
    A cache file with no index entry can never be attributed to an image again -- the id
    in its name says nothing about which picture it came from -- so it is dead weight.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    c.put(imagePath("a.nef"), "recipe1", jpg(0x48));

    const QString stray = QDir(c.cacheDir()).absoluteFilePath("deadbeef.jpg");
    QFile f(stray);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("orphan");
    f.close();

    c.reconcile();

    QVERIFY(!QFile::exists(stray));
    QCOMPARE(c.count(), 1);          // the real entry is untouched
}


void tst_devpreview::reusedPathDoesNotServeThePreviousImage()
{
/*
    The recipe hash identifies a RECIPE, not an image. Presets, Paste Settings and
    multi-image edits give whole folders a byte-identical recipe, so the hash cannot tell
    two images apart at all. If a path is reused by a different image behind Winnow's back
    -- renamed in Finder while it was closed, restored from a backup, renumbered by the
    camera -- both the path and the hash still match, and the previous occupant's picture
    would be served as though it were correct.

    The entry also records the source image's length and modification time, which is what
    makes this a miss.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString p = imagePath("DSC_0001.nef");

    // the first image, and its preview
    {
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("first image");
        f.close();
    }
    const QByteArray payload = jpg(0x71);
    c.put(p, "sharedRecipe", payload);
    QCOMPARE(c.get(p, "sharedRecipe"), payload);

    // a DIFFERENT image takes the same path, carrying the same recipe
    QVERIFY(QFile::remove(p));
    {
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("a completely different image");
        f.close();
    }

    QVERIFY(c.get(p, "sharedRecipe").isEmpty());
    QVERIFY(!c.contains(p, "sharedRecipe"));
    // and the entry is gone rather than left to be matched again
    QCOMPARE(c.count(), 0);
}

void tst_devpreview::pathSpellingDoesNotSplitTheEntry()
{
/*
    The same file arrives spelled several ways -- QFileInfo::filePath() from a folder
    scan, dropDir + "/" + name from a paste (which doubles a separator when the folder
    already ends in one), QUrl::toLocalFile() from a Finder drag, and either case on the
    case-insensitive filesystems both supported platforms use by default.

    Keyed byte-exactly, each spelling is a different image: the same picture is stored
    TWICE at several MB each so the byte cap bites sooner, lookups under the other
    spelling silently re-decode the raw, and the FileOps move/delete hooks quietly find
    nothing to update. All of them are one entry now (Cache/pathkey.h).
*/
    DevPreviewCache &c = DevPreviewCache::instance();

    const QString canonical = imagePath("Spelling.nef");
    {
        QFile f(canonical);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
    }
    const QByteArray payload = jpg(0x74);
    c.put(canonical, "recipe1", payload);
    QCOMPARE(c.count(), 1);

    // every one of these names the same file
    QStringList spellings;
    spellings << QDir(tmp.path()).absoluteFilePath("./Spelling.nef")     // dot component
              << tmp.path() + "//Spelling.nef"                           // doubled slash
              << canonical.toLower();                                    // case
#ifdef Q_OS_WIN
    spellings << QDir::toNativeSeparators(canonical);                    // backslashes
#endif

    for (const QString &sp : spellings) {
        QVERIFY2(c.contains(sp, "recipe1"), qPrintable(sp));
        QCOMPARE(c.get(sp, "recipe1"), payload);
    }

    // and a put under another spelling UPDATES the entry rather than adding a second
    const QByteArray payload2 = jpg(0x75);
    c.put(spellings.first(), "recipe2", payload2);
    QCOMPARE(c.count(), 1);
    QCOMPARE(c.get(canonical, "recipe2"), payload2);
    QCOMPARE(c.totalBytes(), qint64(payload2.size()));

    // one payload on disk, not two
    QDir cacheDir(c.cacheDir());
    QCOMPARE(cacheDir.entryList(QStringList() << "*.jpg", QDir::Files).size(), 1);
}

void tst_devpreview::concurrentGetsAreNotSerialised()
{
/*
    get() must not hold the cache mutex while it reads the payload. Every one of
    ImageCache's decoderCount threads comes through here, and at full sensor resolution
    the read is multi-MB; serialising them would undo the parallel read-ahead that makes
    browsing developed raws fast, and a put() (which writes a payload the same way) would
    stall all of them.

    Concurrency is hard to assert on directly without being flaky, so this pins the part
    that matters and is deterministic: many threads reading at once all get the right
    bytes, and the accounting survives it.
*/
    DevPreviewCache &c = DevPreviewCache::instance();

    QStringList paths;
    QList<QByteArray> payloads;
    for (int i = 0; i < 8; ++i) {
        const QString p = imagePath(QString("conc%1.nef").arg(i));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
        paths << p;
        payloads << jpg(0x80 + i, 512);
        c.put(p, "recipe1", payloads.last());
    }
    QCOMPARE(c.count(), 8);

    QAtomicInt mismatches(0);
    QList<QFuture<void>> running;
    for (int rep = 0; rep < 4; ++rep) {
        for (int i = 0; i < paths.size(); ++i) {
            running << QtConcurrent::run([&, i] {
                if (c.get(paths.at(i), "recipe1") != payloads.at(i))
                    mismatches.fetchAndAddOrdered(1);
            });
        }
    }
    for (QFuture<void> &f : running) f.waitForFinished();

    QCOMPARE(mismatches.loadAcquire(), 0);
    QCOMPARE(c.count(), 8);
}

void tst_devpreview::legacyJsonIndexIsImported()
{
/*
    The index used to be a JSON array. Users upgrading have one, with cache files beside
    it. If the database opened empty, reconcile() would delete every one of those payloads
    -- minutes of re-rendering per folder, for a change meant to be invisible.

    The JSON is renamed rather than deleted, so a bad import is recoverable by hand.
*/
    DevPreviewCache &c = DevPreviewCache::instance();

    // stand in for a previous version's cache: one payload plus the JSON naming it
    const QString p = imagePath("legacy.nef");
    const QByteArray payload = jpg(0x72);
    const QString cacheDir = cacheTmp.path();
    {
        QFile f(QDir(cacheDir).absoluteFilePath("7.jpg"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(payload);
        f.close();
    }
    QJsonObject e;
    e.insert("path", p);
    e.insert("id", 7);
    e.insert("hash", "legacyRecipe");
    e.insert("bytes", double(payload.size()));
    e.insert("used", 1000);
    e.insert("live", true);
    e.insert("vol", "");
    QJsonObject root;
    root.insert("version", 1);
    root.insert("nextId", 8);
    root.insert("entries", QJsonArray{e});
    const QString jsonPath = QDir(cacheDir).absoluteFilePath("index.json");
    {
        QFile f(jsonPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.close();
    }

    // first use of the new database imports it
    c.setCacheDir(QString());
    c.setCacheDir(cacheDir);

    QCOMPARE(c.count(), 1);
    QCOMPARE(c.get(p, "legacyRecipe"), payload);
    QCOMPARE(c.totalBytes(), qint64(payload.size()));

    // the JSON is set aside, not deleted, and not imported twice
    QVERIFY(!QFile::exists(jsonPath));
    QVERIFY(QFile::exists(jsonPath + ".migrated"));

    // ids continue past the imported one rather than clobbering it
    c.put(imagePath("after.nef"), "recipe1", jpg(0x73));
    QCOMPARE(c.count(), 2);
    QCOMPARE(c.get(p, "legacyRecipe"), payload);
}

void tst_devpreview::isCachePathIdentifiesTheProtectedFolder()
{
/*
    The cache folder is browsable, so every write path asks isCachePath before it acts.
    A false negative silently lets an edit corrupt the cache; a false positive would
    refuse writes to ordinary photo folders. Both directions are checked, including the
    case-insensitive match the two supported platforms need and a sibling folder whose
    name merely starts with the cache folder's.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const QString d = cacheTmp.path();

    QVERIFY(c.isCachePath(d));
    QVERIFY(c.isCachePath(d + "/0000001.jpg"));
    QVERIFY(c.isCachePath(d + "/index.db"));
    QVERIFY(c.isCachePath(d.toUpper()));
    QVERIFY(c.isCachePath(d + "/./0000001.jpg"));

    QVERIFY(!c.isCachePath(QString()));
    QVERIFY(!c.isCachePath(imagePath("a.nef")));
    // a sibling whose path is a string prefix of the cache folder is NOT inside it
    QVERIFY(!c.isCachePath(d + "Extra/a.jpg"));
}

QTEST_MAIN(tst_devpreview)
#include "tst_devpreview.moc"
