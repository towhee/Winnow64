#include <QtTest>
#include <QBuffer>
#include <QImage>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlDatabase>

#include "Cache/cachedb.h"
#include "Cache/thumbcache.h"
#include "Main/global.h"

/*
    THE THUMBNAIL INDEX.

    Cache/thumbcache.h is what makes scrolling an unvisited region of a 250,000
    image catalog affordable: the icon is decoded once, ever, and after that a
    scroll is a database read rather than a file open, a segment walk and a JPEG
    decode per row.

    The cases here are the ones where a cache can be actively WRONG rather than
    merely empty:

      o a source file edited outside Winnow keeps its path, so a thumbnail
        returned on identity alone would be yesterday's picture for today's
        file. That is the one failure a cache must not have;
      o a sweep that DELETED rather than demoted would throw away the
        thumbnails for a whole volume the moment it was unplugged;
      o eviction that ignored the demoted flag would drop live entries while
        keeping dead ones.
*/
class tst_thumbcache : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void schemaHasTheThumbTable();
    void anOlderIndexMigratesUpToTheThumbTable();
    void roundTripsAJpeg();
    void aChangedSourceIsAMissNotAStalePicture();
    void rowsWithoutAStampAreGrandfathered();
    void pathSpellingsFoldToOneRow();
    void putReplacesRatherThanDuplicating();
    void evictionDropsDemotedFirstThenLeastRecentlyUsed();
    void sweepDemotesMissingFilesAndRevivesReturningOnes();
    void moveAndDeleteFollowTheFile();
    void putImageQueuesAndSkipsWhatIsAlreadyThere();
    void theDevelopGateIsPerImageNotPerMode();
    void getImageReturnsAPaintableIcon();

private:
    QTemporaryDir *tmp = nullptr;
    static QByteArray jpegOf(int w, int h, Qt::GlobalColor c)
    {
        QImage im(w, h, QImage::Format_RGB32);
        im.fill(c);
        QByteArray out;
        QBuffer buf(&out);
        buf.open(QIODevice::WriteOnly);
        im.save(&buf, "JPG", 85);
        return out;
    }
    QString makeFile(const QString &name, int bytes = 64) const
    {
        const QString p = tmp->filePath(name);
        QFile f(p);
        f.open(QIODevice::WriteOnly);
        f.write(QByteArray(bytes, 'x'));
        f.close();
        return p;
    }
    static QPair<qint64, qint64> stampOf(const QString &p)
    {
        const QFileInfo fi(p);
        return { fi.size(), fi.lastModified().toSecsSinceEpoch() };
    }
};

void tst_thumbcache::initTestCase()
{
    tmp = new QTemporaryDir;
    QVERIFY(tmp->isValid());
    CacheDb::instance().setPath(tmp->filePath("index.db"));
    QVERIFY2(CacheDb::instance().db().isOpen(), "index database did not open");
}

void tst_thumbcache::init()
{
    ThumbCache::instance().clear();
    ThumbCache::instance().setMaxBytes(5LL * 1024 * 1024 * 1024);
}

void tst_thumbcache::cleanupTestCase() { delete tmp; }

void tst_thumbcache::schemaHasTheThumbTable()
{
    /*  Ask the real schema rather than a copy pasted into the test -- a copy
        would be one more hand-maintained list, which is the problem
        Datamodel/rowfields.h exists to avoid. */
    QSqlQuery q(CacheDb::instance().db());
    QVERIFY(q.exec("PRAGMA table_info(thumb)"));
    QSet<QString> cols;
    while (q.next()) cols.insert(q.value(1).toString());
    QVERIFY2(!cols.isEmpty(), "no thumb table: the schema did not migrate");
    for (const QString &c : { "pathkey", "path", "folder", "jpg", "w", "h",
                              "bytes", "used", "live", "vol", "srcsize", "srcmtime" })
        QVERIFY2(cols.contains(c), qPrintable("thumb is missing " + c));
}

void tst_thumbcache::anOlderIndexMigratesUpToTheThumbTable()
{
    /*  EVERY EXISTING USER HAS A VERSION 4 INDEX, so the migration is the path
        that actually runs in the field -- a fresh database proves nothing about
        it. Rather than hand-write the v1-v4 DDL here (which would be one more
        copy of the schema, the very thing rowfields.h exists to avoid), take
        the real migrated file, put it BACK to the older shape, and reopen.

        The migration is additive and guarded by `if (version < 5)`, so an
        existing catalog keeps its images, keywords and devpreview rows and
        simply gains an empty thumb table. */
    QSqlDatabase db = CacheDb::instance().db();
    QVERIFY(db.isOpen());
    QSqlQuery q(db);
    QVERIFY(q.exec("DROP TABLE thumb"));
    QVERIFY(q.exec("PRAGMA user_version = 4"));

    CacheDb::instance().closeThisThread();
    QSqlDatabase back = CacheDb::instance().db();
    QVERIFY2(back.isOpen(), "the older index failed to reopen");

    QSqlQuery v(back);
    QVERIFY(v.exec("PRAGMA user_version") && v.next());
    QCOMPARE(v.value(0).toInt(), CacheDb::schemaVersion());

    QVERIFY(v.exec("PRAGMA table_info(thumb)"));
    QVERIFY2(v.next(), "migrating a version 4 index did not create the thumb table");

    /*  The tables that were already there are still there -- an additive
        migration that quietly dropped the catalog would be a catastrophe that
        a "does thumb exist" check alone would not notice. */
    for (const char *t : { "image", "keyword", "image_keyword", "devpreview" }) {
        QVERIFY(v.exec(QString("PRAGMA table_info(%1)").arg(t)));
        QVERIFY2(v.next(), qPrintable(QString("migration lost table %1").arg(t)));
    }
}

void tst_thumbcache::roundTripsAJpeg()
{
    ThumbCache &t = ThumbCache::instance();
    const QString p = makeFile("a.jpg");
    const auto st = stampOf(p);
    const QByteArray jpg = jpegOf(256, 170, Qt::red);

    QVERIFY(!t.contains(p, st.first, st.second));
    QVERIFY(t.get(p, st.first, st.second).isEmpty());

    t.put(p, jpg, 256, 170, st.first, st.second);
    QVERIFY(t.contains(p, st.first, st.second));
    QCOMPARE(t.get(p, st.first, st.second), jpg);
    QCOMPARE(t.count(), 1);
    QCOMPARE(t.totalBytes(), qint64(jpg.size()));

    // it really is a decodable image on the way back out
    QImage back;
    QVERIFY(back.loadFromData(t.get(p, st.first, st.second), "JPG"));
    QCOMPARE(back.size(), QSize(256, 170));
}

void tst_thumbcache::aChangedSourceIsAMissNotAStalePicture()
{
    /*  THE CASE THAT MATTERS. An image edited by another program keeps its
        path, so identity alone would hand back a picture of the file as it used
        to be. Every row carries the source's size and mtime and a mismatch is a
        miss. */
    ThumbCache &t = ThumbCache::instance();
    const QString p = makeFile("b.jpg", 100);
    const auto before = stampOf(p);
    t.put(p, jpegOf(64, 64, Qt::blue), 64, 64, before.first, before.second);
    QVERIFY(!t.get(p, before.first, before.second).isEmpty());

    // same path, different bytes
    QVERIFY(t.get(p, before.first + 1, before.second).isEmpty());
    QVERIFY(t.get(p, before.first, before.second + 1).isEmpty());
    QVERIFY(!t.contains(p, before.first + 1, before.second));
    // ... and the unchanged file still hits
    QVERIFY(!t.get(p, before.first, before.second).isEmpty());
}

void tst_thumbcache::rowsWithoutAStampAreGrandfathered()
{
    /*  A row written before the stamp existed records zeros. Re-decoding a
        quarter of a million thumbnails to close a rare window is the worse
        trade, so those rows are trusted -- the same call DevPreviewCache makes
        for the rows it imported from its JSON index. */
    ThumbCache &t = ThumbCache::instance();
    const QString p = makeFile("c.jpg");
    t.put(p, jpegOf(32, 32, Qt::green), 32, 32, 0, 0);
    QVERIFY(!t.get(p, 12345, 67890).isEmpty());
    QVERIFY(t.contains(p, 12345, 67890));
}

void tst_thumbcache::pathSpellingsFoldToOneRow()
{
    /*  The same file reaches Winnow spelled several ways -- a doubled slash
        from a concatenated destination, a different drive-letter case, an
        uncleaned relative segment. cachePathKey folds them; without it put()
        under a second spelling inserts a SECOND row and get() under it
        re-decodes. */
    ThumbCache &t = ThumbCache::instance();
    const QString p = makeFile("d.jpg");
    const auto st = stampOf(p);
    t.put(p, jpegOf(32, 32, Qt::yellow), 32, 32, st.first, st.second);
    QCOMPARE(t.count(), 1);

    const QString doubled = tmp->path() + "//d.jpg";
    const QString dotted  = tmp->path() + "/./d.jpg";
    QVERIFY(!t.get(doubled, st.first, st.second).isEmpty());
    QVERIFY(!t.get(dotted, st.first, st.second).isEmpty());

    t.put(doubled, jpegOf(32, 32, Qt::cyan), 32, 32, st.first, st.second);
    QCOMPARE(t.count(), 1);         // replaced, not duplicated
}

void tst_thumbcache::putReplacesRatherThanDuplicating()
{
    ThumbCache &t = ThumbCache::instance();
    const QString p = makeFile("e.jpg");
    const auto st = stampOf(p);
    const QByteArray small = jpegOf(32, 32, Qt::red);
    const QByteArray big = jpegOf(256, 256, Qt::blue);

    t.put(p, small, 32, 32, st.first, st.second);
    t.put(p, big, 256, 256, st.first, st.second);
    QCOMPARE(t.count(), 1);
    QCOMPARE(t.get(p, st.first, st.second), big);
    QCOMPARE(t.totalBytes(), qint64(big.size()));
}

void tst_thumbcache::evictionDropsDemotedFirstThenLeastRecentlyUsed()
{
    ThumbCache &t = ThumbCache::instance();
    const QByteArray jpg = jpegOf(64, 64, Qt::magenta);

    QStringList paths;
    for (int i = 0; i < 6; ++i) {
        const QString p = makeFile(QString("ev%1.jpg").arg(i));
        paths << p;
        const auto st = stampOf(p);
        t.put(p, jpg, 64, 64, st.first, st.second);
    }
    QCOMPARE(t.count(), 6);

    /*  Demote two of them by hand -- what the sweep does to a row whose source
        has gone. They must be the first to go. */
    QSqlQuery q(CacheDb::instance().db());
    q.exec("UPDATE thumb SET live = 0 WHERE path LIKE '%ev0.jpg' OR path LIKE '%ev1.jpg'");

    t.setMaxBytes(qint64(jpg.size()) * 4);
    const QString p = makeFile("ev6.jpg");
    const auto st = stampOf(p);
    t.put(p, jpg, 64, 64, st.first, st.second);      // triggers eviction

    QVERIFY2(t.totalBytes() <= qint64(jpg.size()) * 4, "cap not honoured");

    /*  Ask the table whether the ROW is there, not contains() -- contains()
        applies the staleness stamp, so passing a stamp of 0,0 for a row written
        with a real one returns false whether the row exists or not, and the
        assertion would pass without testing anything. Caught by writing the
        weaker version first. */
    auto rowExists = [](const QString &path) {
        QSqlQuery q(CacheDb::instance().db());
        q.prepare("SELECT COUNT(*) FROM thumb WHERE path = ?");
        q.addBindValue(path);
        return q.exec() && q.next() && q.value(0).toInt() > 0;
    };
    // the two demoted rows went first
    QVERIFY2(!rowExists(paths.at(0)), "a demoted row survived eviction");
    QVERIFY2(!rowExists(paths.at(1)), "a demoted row survived eviction");
    // the newest is still there
    QVERIFY2(rowExists(p), "the most recently used row was evicted");
}

void tst_thumbcache::sweepDemotesMissingFilesAndRevivesReturningOnes()
{
    /*  DEMOTE, NEVER DELETE. A file restored from the trash must find its
        thumbnail again; and a row whose VOLUME is absent is skipped entirely,
        so ejecting a card does not throw away everything on it. (The
        unmounted-volume half is not exercisable here -- the temp dir is always
        mounted -- which is why the rule is written down at the call site.) */
    ThumbCache &t = ThumbCache::instance();
    const QString keep = makeFile("keep.jpg");
    const QString gone = makeFile("gone.jpg");
    const auto sk = stampOf(keep);
    const auto sg = stampOf(gone);
    t.put(keep, jpegOf(32, 32, Qt::red), 32, 32, sk.first, sk.second);
    t.put(gone, jpegOf(32, 32, Qt::blue), 32, 32, sg.first, sg.second);

    QVERIFY(QFile::remove(gone));
    QCOMPARE(t.sweep(), 1);
    QCOMPARE(t.count(), 2);                 // demoted, not deleted

    QSqlQuery q(CacheDb::instance().db());
    QVERIFY(q.exec("SELECT COUNT(*) FROM thumb WHERE live = 0"));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);

    // back from the trash
    makeFile("gone.jpg");
    QCOMPARE(t.sweep(), 0);
    QVERIFY(q.exec("SELECT COUNT(*) FROM thumb WHERE live = 0"));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
}

void tst_thumbcache::moveAndDeleteFollowTheFile()
{
    ThumbCache &t = ThumbCache::instance();
    const QString src = makeFile("m1.jpg");
    const auto st = stampOf(src);
    const QByteArray jpg = jpegOf(48, 48, Qt::darkGreen);
    t.put(src, jpg, 48, 48, st.first, st.second);

    const QString dst = tmp->filePath("m2.jpg");
    t.onMoved(src, dst);
    QVERIFY(t.get(src, st.first, st.second).isEmpty());
    QCOMPARE(t.get(dst, st.first, st.second), jpg);
    QCOMPARE(t.count(), 1);

    t.onDeleted(dst);
    QCOMPARE(t.count(), 0);
}

void tst_thumbcache::putImageQueuesAndSkipsWhatIsAlreadyThere()
{
    /*  putImage() is ASYNCHRONOUS -- it hands the image to one batching writer
        thread and returns, because doing the encode and the insert inline on
        the loader thread was measured at +47% on the icon path (the numbers are
        in Cache/thumbcache.h). flush() is what makes the write observable, here
        and at shutdown.

        The second half is the one that matters for cost: a folder revisited
        must not re-encode everything in it. The writer checks the index before
        encoding, so the second putImage of the same unchanged file writes
        nothing. */
    ThumbCache &t = ThumbCache::instance();
    G::cacheThumbnails = true;

    const QString p = makeFile("q1.jpg");
    const auto st = stampOf(p);
    QImage im(120, 80, QImage::Format_RGB32);
    im.fill(Qt::darkCyan);

    t.putImage(p, im, false);
    t.flush();
    QVERIFY2(t.contains(p, st.first, st.second), "putImage did not reach the index");
    const qint64 afterFirst = t.totalBytes();
    QVERIFY(afterFirst > 0);

    /*  Same file, unchanged: the writer must SKIP it rather than re-encode.
        Checked with written(), not with count() or totalBytes() -- re-encoding
        an unchanged image produces byte-identical output, so neither of those
        can tell a skip from a rewrite. The weaker version was written first and
        deleting the skip did not fail it. */
    const qint64 wroteOnce = t.written();
    t.putImage(p, im, false);
    t.flush();
    QCOMPARE(t.count(), 1);
    QCOMPARE(t.totalBytes(), afterFirst);
    QVERIFY2(t.written() == wroteOnce, "an unchanged image was re-encoded");

    /*  And the switch really switches it off -- the preference has to mean
        something, not just persist. */
    G::cacheThumbnails = false;
    const QString p2 = makeFile("q2.jpg");
    t.putImage(p2, im, false);
    t.flush();
    QCOMPARE(t.count(), 1);
    G::cacheThumbnails = true;
}

void tst_thumbcache::theDevelopGateIsPerImageNotPerMode()
{
    /*  THE BUG THIS ALMOST SHIPPED WITH. Thumb::loadThumb returns the DEVELOPED
        thumbnail rather than the camera's for an edited image in a
        developed-showing mode, so caching or serving the camera's picture there
        would show the user the wrong one. The first version gated on the MODE
        alone -- and G::previewSource DEFAULTS to Developed, so the cache was
        switched off entirely and silently. It measured as "no change", which is
        exactly what a feature that never runs looks like.

        The test is per IMAGE: an unedited image has no developed thumbnail at
        any setting, so it is always cacheable; only an EDITED image in a
        developed-showing mode is excluded. */
    const auto mode = G::operationMode;
    const auto src = G::previewSource;

    G::operationMode = G::OperationMode::Preview;
    G::previewSource = G::PreviewSource::Developed;      // the DEFAULT
    QVERIFY2(ThumbCache::wantsOriginalThumb(false),
             "an unedited image must be cacheable at the default setting");
    QVERIFY2(!ThumbCache::wantsOriginalThumb(true),
             "an edited image must not be cached while developed is shown");

    G::previewSource = G::PreviewSource::Original;
    QVERIFY(ThumbCache::wantsOriginalThumb(false));
    QVERIFY2(ThumbCache::wantsOriginalThumb(true),
             "showing originals, the camera thumbnail is right even for an edit");

    /*  Develop mode always shows developed -- you cannot edit what you cannot
        see -- whatever the preview source says. */
    G::operationMode = G::OperationMode::Develop;
    QVERIFY(ThumbCache::wantsOriginalThumb(false));
    QVERIFY(!ThumbCache::wantsOriginalThumb(true));

    G::operationMode = mode;
    G::previewSource = src;
}

void tst_thumbcache::getImageReturnsAPaintableIcon()
{
    /*  The read half end to end: what comes back has to be usable as an icon
        without the caller knowing where it came from -- RGB32, no larger than
        G::maxIconSize, and the same picture that went in. */
    ThumbCache &t = ThumbCache::instance();
    G::cacheThumbnails = true;
    const QString p = makeFile("read1.jpg");

    QImage in(200, 133, QImage::Format_RGB32);
    in.fill(Qt::darkRed);
    t.putImage(p, in, false);
    t.flush();

    const QImage out = t.getImage(p, false);
    QVERIFY2(!out.isNull(), "a stored thumbnail did not come back");
    QCOMPARE(out.size(), QSize(200, 133));
    QCOMPARE(out.format(), QImage::Format_RGB32);

    /*  A miss is a null image, not an empty one -- the caller tests isNull() to
        decide whether to fall through and decode. */
    QVERIFY(t.getImage(tmp->filePath("never-stored.jpg"), false).isNull());

    /*  And an edited image in a developed-showing mode does not get served the
        camera's thumbnail. */
    const auto src = G::previewSource;
    G::previewSource = G::PreviewSource::Developed;
    QVERIFY(t.getImage(p, /*hasDevelopRecipe*/true).isNull());
    QVERIFY(!t.getImage(p, /*hasDevelopRecipe*/false).isNull());
    G::previewSource = src;
}

QTEST_MAIN(tst_thumbcache)
#include "tst_thumbcache.moc"
