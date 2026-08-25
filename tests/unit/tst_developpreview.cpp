#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "Cache/developpreviewcache.h"

/*
    DevelopPreviewCache -- the on-disk cache of screen-resolution develop previews.

    The behaviours worth pinning are the ones that protect the user's data or their
    patience, not the plumbing:

      o a preview is never served for a recipe it was not rendered from
      o a rename costs an index edit, not a file rewrite
      o the orphan sweep NEVER acts on an unmounted volume (an ejected card must not
        look like a folder full of deleted images)
      o the byte cap actually bounds the cache, evicting demoted entries first
*/
class tst_developpreview : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void putGetRoundTrip();
    void staleRecipeMisses();
    void movePreservesPreviewWithoutRewritingIt();
    void deleteRemovesEntryAndFile();
    void capEvictsAndPrefersDemoted();
    void sweepDemotesMissingSource();
    void sweepSkipsUnmountedVolume();
    void indexSurvivesReload();
    void lazyLoadPreservesIdsAcrossSessions();
    void reconcileDropsStrayFiles();

private:
    QString imagePath(const QString &name) const;
    static QByteArray jpg(int fill, int kb = 4);

    QTemporaryDir tmp;          // stands in for the image folder
    QTemporaryDir cacheTmp;     // stands in for AppDataLocation/PreviewCache
};

QString tst_developpreview::imagePath(const QString &name) const
{
    return QDir(tmp.path()).absoluteFilePath(name);
}

/* A payload that is merely bytes -- the cache never decodes what it stores. */
QByteArray tst_developpreview::jpg(int fill, int kb)
{
    return QByteArray(kb * 1024, char(fill));
}

void tst_developpreview::init()
{
    QVERIFY(tmp.isValid());
    QVERIFY(cacheTmp.isValid());
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
    c.clear();
    c.setCacheDir(cacheTmp.path());
    c.setMaxBytes(2LL * 1024 * 1024 * 1024);
    c.clear();
}

void tst_developpreview::putGetRoundTrip()
{
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
    const QString p = imagePath("a.nef");
    const QByteArray payload = jpg(0x41);

    c.put(p, "recipe1", payload);
    QVERIFY(c.contains(p, "recipe1"));
    QCOMPARE(c.get(p, "recipe1"), payload);
    QCOMPARE(c.count(), 1);
    QCOMPARE(c.totalBytes(), qint64(payload.size()));
}

void tst_developpreview::staleRecipeMisses()
{
/*
    The whole point of storing the recipe hash: after an edit (or an edit made by another
    machine and synced in) the cached pixels describe a picture the user has moved on
    from. Showing them would be worse than showing nothing.
*/
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
    const QString p = imagePath("a.nef");

    c.put(p, "recipe1", jpg(0x41));
    QVERIFY(c.get(p, "recipe2").isEmpty());
    QVERIFY(!c.contains(p, "recipe2"));
    // the entry is still there for its own recipe -- a miss is not an eviction
    QVERIFY(!c.get(p, "recipe1").isEmpty());
}

void tst_developpreview::movePreservesPreviewWithoutRewritingIt()
{
/*
    Cache files are named by an opaque id precisely so a rename or a move is an index
    edit. If this ever regresses into "rename the file on disk", moving a folder of
    edited images becomes O(n) file operations for no benefit.
*/
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
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

void tst_developpreview::deleteRemovesEntryAndFile()
{
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
    const QString p = imagePath("a.nef");
    c.put(p, "recipe1", jpg(0x43));
    QCOMPARE(c.count(), 1);

    c.onDeleted(p);

    QCOMPARE(c.count(), 0);
    QCOMPARE(c.totalBytes(), qint64(0));
    QDir cacheDir(c.cacheDir());
    QVERIFY(cacheDir.entryList(QStringList() << "*.jpg", QDir::Files).isEmpty());
}

void tst_developpreview::capEvictsAndPrefersDemoted()
{
/*
    The cap is the load-bearing bound -- the sweep only reclaims previews whose source is
    gone, while the ordinary case is thousands of images that all still exist. Demoted
    entries (source missing at the last sweep) must go first.
*/
    DevelopPreviewCache &c = DevelopPreviewCache::instance();

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

void tst_developpreview::sweepDemotesMissingSource()
{
/*
    Demote, never delete: a file that comes back from the trash finds its preview intact.
*/
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
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

void tst_developpreview::sweepSkipsUnmountedVolume()
{
/*
    THE important one. Winnow browses memory cards and external drives constantly. If the
    sweep treated "path does not exist" on an unmounted volume as a deletion, ejecting a
    drive and restarting would demote -- and then evict -- every preview on it.

    Simulated by writing an entry whose recorded mount point does not exist, which is
    exactly the state an ejected volume leaves behind.
*/
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
    const QString p = imagePath("oncard.nef");
    c.put(p, "recipe1", jpg(0x46));
    c.save();

    // rewrite the index so the entry claims to live on a volume that is not mounted
    const QString indexPath = QDir(c.cacheDir()).absoluteFilePath("index.json");
    QFile idx(indexPath);
    QVERIFY(idx.open(QIODevice::ReadOnly));
    QJsonObject root = QJsonDocument::fromJson(idx.readAll()).object();
    idx.close();
    QJsonArray entries = root.value("entries").toArray();
    QCOMPARE(entries.size(), 1);
    QJsonObject e = entries.at(0).toObject();
    e.insert("vol", "/Volumes/__WinnowNoSuchVolume__");
    entries.replace(0, e);
    root.insert("entries", entries);
    QVERIFY(idx.open(QIODevice::WriteOnly));
    idx.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    idx.close();

    c.setCacheDir(QString());               // force a genuine reload below
    c.setCacheDir(cacheTmp.path());
    QCOMPARE(c.count(), 1);

    // the source does not exist (it never did) -- but the volume is not mounted
    QCOMPARE(c.sweep(), 0);
    QVERIFY(!c.get(p, "recipe1").isEmpty());
}

void tst_developpreview::indexSurvivesReload()
{
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
    const QString p = imagePath("a.nef");
    const QByteArray payload = jpg(0x47);
    c.put(p, "recipe1", payload);
    c.save();

    c.setCacheDir(QString());               // point elsewhere, then back
    c.setCacheDir(cacheTmp.path());

    QCOMPARE(c.count(), 1);
    QCOMPARE(c.get(p, "recipe1"), payload);
}

void tst_developpreview::lazyLoadPreservesIdsAcrossSessions()
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
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
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

void tst_developpreview::reconcileDropsStrayFiles()
{
/*
    A cache file with no index entry can never be attributed to an image again -- the id
    in its name says nothing about which picture it came from -- so it is dead weight.
*/
    DevelopPreviewCache &c = DevelopPreviewCache::instance();
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

QTEST_MAIN(tst_developpreview)
#include "tst_developpreview.moc"
