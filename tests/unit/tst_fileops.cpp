#include <QtTest>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "Utilities/fileops.h"
#include "Cache/cachedb.h"
#include "Cache/devpreviewcache.h"

/*
    FileOps -- the single choke point every image file operation goes through.

    An image in Winnow is the image PLUS its sidecars, and the sidecar holds the entire
    Develop recipe. Before FileOps that pairing was reimplemented four different ways and
    they disagreed; these tests pin the one definition.

    The trash tests point FileOps::setTrashHook at a temp folder; nothing here may reach
    the user's real Trash.
*/
class tst_fileops : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void companionsFindsXmpAndTxt();
    void companionsIsCaseInsensitive();
    void companionsExcludesPairedImages();
    void copyCarriesCompanions();
    void copyRenamesCompanionsToDestinationBase();
    void moveCarriesCompanionsAndPreview();
    void refusesToWriteInThePreviewCacheFolder();
    void trashFilesCarriesSidecarsNotPairs();
    void trashFilesSortsMissingAndProtected();
    void trashFilesCancelsBetweenChunks();
    void sidecarPathByFormat();
    void legacySidecarReadWhenAlone();
    void legacySidecarBelongsToTheRaw();
    void prepareMovesLegacyWhenAlone();
    void prepareCopiesLegacySharedByFullNameImages();
    void prepareLeavesTheRawsSidecarAlone();
    void companionsSplitARawJpegPair();
    void trashingTheJpegKeepsTheRawsSidecar();
    void moveRenamesFullNameSidecar();

private:
    QString p(const QString &name) const;
    void touch(const QString &name, const QByteArray &data = "x") const;

    QTemporaryDir tmp;
    QTemporaryDir cacheTmp;
    QTemporaryDir trashTmp;         // the fake Trash the trash hook moves into
    void useFakeTrash();
};

QString tst_fileops::p(const QString &name) const
{
    return QDir(tmp.path()).absoluteFilePath(name);
}

void tst_fileops::touch(const QString &name, const QByteArray &data) const
{
    QFile f(p(name));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
    f.close();
}

void tst_fileops::initTestCase()
{
    /* init() clears the preview cache before pointing it anywhere; without this that
       resolves to the real AppDataLocation. See tst_devpreview. */
    QStandardPaths::setTestModeEnabled(true);
}

void tst_fileops::useFakeTrash()
{
    QVERIFY(trashTmp.isValid());
    QDir t(trashTmp.path());
    for (const QString &f : t.entryList(QDir::Files | QDir::Hidden)) t.remove(f);
    const QString dir = trashTmp.path();
    FileOps::setTrashHook([dir](const QString &path) {
        return QFile::rename(path, QDir(dir).absoluteFilePath(QFileInfo(path).fileName()));
    });
}

void tst_fileops::cleanupTestCase()
{
    FileOps::setTrashHook({});
    /* Release the index database before QTemporaryDir removes the directory under it. */
    CacheDb::instance().closeThisThread();
}

void tst_fileops::init()
{
    QVERIFY(tmp.isValid());
    QVERIFY(cacheTmp.isValid());

    // start each test from an empty folder
    QDir d(tmp.path());
    for (const QString &f : d.entryList(QDir::Files | QDir::Hidden)) d.remove(f);

    DevPreviewCache &c = DevPreviewCache::instance();
    c.clear();
    c.setCacheDir(cacheTmp.path());
    c.clear();
}

void tst_fileops::companionsFindsXmpAndTxt()
{
/*
    The .txt sidecar is the one the old code kept losing: only two of the four sidecar
    implementations handled it, so a drag-and-drop or an ingest silently left it behind.
*/
    touch("DSC_001.NEF");
    touch("DSC_001.xmp");
    touch("DSC_001.txt");
    touch("DSC_002.xmp");        // belongs to a different image

    QStringList c = FileOps::companions(p("DSC_001.NEF"));
    c.sort();
    QCOMPARE(c.size(), 2);
    QCOMPARE(QFileInfo(c.at(0)).fileName(), QString("DSC_001.txt"));
    QCOMPARE(QFileInfo(c.at(1)).fileName(), QString("DSC_001.xmp"));
}

void tst_fileops::companionsIsCaseInsensitive()
{
/*
    Other applications write .XMP. The old hardcoded "<base>.xmp" probe missed those
    entirely, so the image moved and its ratings and develop recipe did not.
*/
    touch("DSC_010.NEF");
    touch("DSC_010.XMP");

    const QStringList c = FileOps::companions(p("DSC_010.NEF"));
    QCOMPARE(c.size(), 1);
    QCOMPARE(QFileInfo(c.at(0)).fileName(), QString("DSC_010.XMP"));
}

void tst_fileops::companionsExcludesPairedImages()
{
/*
    companions() is deliberately narrower than the rename dialog's basename scan. Rename
    DOES want to take the paired JPG of a raw+jpg pair along; trashing a NEF must NOT
    trash its JPG.
*/
    touch("DSC_020.NEF");
    touch("DSC_020.JPG");
    touch("DSC_020.xmp");

    const QStringList c = FileOps::companions(p("DSC_020.NEF"));
    QCOMPARE(c.size(), 1);
    QCOMPARE(QFileInfo(c.at(0)).fileName(), QString("DSC_020.xmp"));
}

void tst_fileops::copyCarriesCompanions()
{
    QDir(tmp.path()).mkdir("dest");
    touch("DSC_030.NEF", "image");
    touch("DSC_030.xmp", "recipe");

    const QString dst = QDir(tmp.path()).absoluteFilePath("dest/DSC_030.NEF");
    QVERIFY(FileOps::copyFile(p("DSC_030.NEF"), dst));

    QVERIFY(QFile::exists(dst));
    QVERIFY(QFile::exists(QDir(tmp.path()).absoluteFilePath("dest/DSC_030.xmp")));
    QVERIFY(QFile::exists(p("DSC_030.NEF")));       // a copy leaves the source alone
    QVERIFY(QFile::exists(p("DSC_030.xmp")));
}

void tst_fileops::copyRenamesCompanionsToDestinationBase()
{
/*
    Ingest renames images to a token template as it copies; the sidecar has to follow the
    NEW base name or it is orphaned at the destination.
*/
    QDir(tmp.path()).mkdir("dest");
    touch("DSC_040.NEF", "image");
    touch("DSC_040.xmp", "recipe");

    const QString dst = QDir(tmp.path()).absoluteFilePath("dest/2026-08-24_0001.NEF");
    QVERIFY(FileOps::copyFile(p("DSC_040.NEF"), dst));

    QVERIFY(QFile::exists(dst));
    const QString movedSidecar =
        QDir(tmp.path()).absoluteFilePath("dest/2026-08-24_0001.xmp");
    QVERIFY(QFile::exists(movedSidecar));

    QFile f(movedSidecar);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("recipe"));
}

void tst_fileops::moveCarriesCompanionsAndPreview()
{
/*
    The end-to-end promise: move an edited image and it keeps its edits (sidecar) AND its
    cached develop preview (index entry), with nothing left behind.
*/
    QDir(tmp.path()).mkdir("dest");
    touch("DSC_050.NEF", "image");
    touch("DSC_050.xmp", "recipe");

    DevPreviewCache &c = DevPreviewCache::instance();
    const QByteArray payload(1024, 'p');
    c.put(p("DSC_050.NEF"), "recipe1", payload);

    const QString dst = QDir(tmp.path()).absoluteFilePath("dest/DSC_050.NEF");
    QVERIFY(FileOps::moveFile(p("DSC_050.NEF"), dst));

    QVERIFY(QFile::exists(dst));
    QVERIFY(QFile::exists(QDir(tmp.path()).absoluteFilePath("dest/DSC_050.xmp")));
    QVERIFY(!QFile::exists(p("DSC_050.NEF")));      // a move leaves nothing behind
    QVERIFY(!QFile::exists(p("DSC_050.xmp")));

    QCOMPARE(c.get(dst, "recipe1"), payload);
    QVERIFY(c.get(p("DSC_050.NEF"), "recipe1").isEmpty());
}

void tst_fileops::refusesToWriteInThePreviewCacheFolder()
{
/*
    The preview cache folder is browsable, so the user can select it and reach every file
    operation from there. Its files are named by an id only the cache index can attribute
    to an image, and the cache DELETES any file its index does not name -- so a move out
    of it detaches a preview from its image, and a copy into it is dropped at the next
    launch along with any sidecar written beside it. Every operation refuses on either
    side of the path, and refusing means changing nothing on disk.
*/
    const QDir cache(cacheTmp.path());
    const QString cached = cache.absoluteFilePath("0000001.jpg");
    QFile f(cached);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("preview");
    f.close();

    touch("DSC_060.NEF");

    // out of the cache
    QVERIFY(!FileOps::moveFile(cached, p("stolen.jpg")));
    QVERIFY(QFile::exists(cached));
    QVERIFY(!QFile::exists(p("stolen.jpg")));

    // into the cache
    const QString intruder = cache.absoluteFilePath("DSC_060.NEF");
    QVERIFY(!FileOps::copyFile(p("DSC_060.NEF"), intruder));
    QVERIFY(!QFile::exists(intruder));
    QVERIFY(!FileOps::moveFile(p("DSC_060.NEF"), intruder));
    QVERIFY(!QFile::exists(intruder));
    QVERIFY(QFile::exists(p("DSC_060.NEF")));

    // and the cache file is not trashed (the one trashFile case safe to run: it refuses)
    QVERIFY(!FileOps::trashFile(cached));
    QVERIFY(QFile::exists(cached));
}

void tst_fileops::trashFilesCarriesSidecarsNotPairs()
{
/*
    The batch path indexes each folder's sidecars once instead of listing the folder per
    image. It must still find mixed-case sidecars, and it must still leave a raw+jpg
    pair's JPG alone. 250 images spans three chunks.
*/
    useFakeTrash();
    QStringList paths;
    for (int i = 0; i < 250; ++i) {
        const QString base = QString("IMG_%1").arg(i, 4, 10, QChar('0'));
        touch(base + ".NEF");
        touch(base + (i % 2 ? ".XMP" : ".xmp"));
        if (i % 10 == 0) touch(base + ".txt");
        paths << p(base + ".NEF");
    }
    touch("IMG_0000.JPG");      // the pair of IMG_0000.NEF: must survive

    int calls = 0;
    const FileOps::TrashResult r = FileOps::trashFiles(paths, [&](int) { ++calls; return true; });

    QCOMPARE(r.trashed.size(), 250);
    QVERIFY(r.failed.isEmpty());
    QVERIFY(r.missing.isEmpty());
    QVERIFY(!r.cancelled);
    QCOMPARE(calls, 3);
    const QStringList left = QDir(tmp.path()).entryList(QDir::Files | QDir::Hidden);
    QCOMPARE(left, QStringList{"IMG_0000.JPG"});
    QCOMPARE(QDir(trashTmp.path()).entryList(QDir::Files).size(), 250 + 250 + 25);
}

void tst_fileops::trashFilesSortsMissingAndProtected()
{
    useFakeTrash();
    touch("DSC_070.NEF");
    const QString cached = QDir(cacheTmp.path()).absoluteFilePath("0000002.jpg");
    QFile f(cached);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("preview");
    f.close();

    const FileOps::TrashResult r =
        FileOps::trashFiles({p("DSC_070.NEF"), p("gone.NEF"), cached});

    QCOMPARE(r.trashed, QStringList{p("DSC_070.NEF")});
    QCOMPARE(r.missing, QStringList{p("gone.NEF")});
    QCOMPARE(r.failed, QStringList{cached});
    QVERIFY(QFile::exists(cached));
}

void tst_fileops::trashFilesCancelsBetweenChunks()
{
/*
    Cancel stops between chunks, and the result names exactly what went -- MW removes
    those rows, so the model must match the disk.
*/
    useFakeTrash();
    QStringList paths;
    for (int i = 0; i < 350; ++i) {
        const QString name = QString("C_%1.JPG").arg(i, 4, 10, QChar('0'));
        touch(name);
        paths << p(name);
    }

    const FileOps::TrashResult r =
        FileOps::trashFiles(paths, [](int done) { return done < 200; });

    QVERIFY(r.cancelled);
    QCOMPARE(r.trashed.size(), 200);
    QCOMPARE(r.trashed, paths.mid(0, 200));
    for (const QString &t : r.trashed) QVERIFY(!QFile::exists(t));
    for (const QString &k : paths.mid(200)) QVERIFY(QFile::exists(k));
}

/* ---------------------------------------------------------------------------------
   Sidecar naming. Each test works in its own subfolder: FileOps caches a folder's
   listing keyed on the folder's mtime, and a fresh folder cannot be served a listing
   from the test before.
   --------------------------------------------------------------------------------- */

namespace {
QString sub(const QTemporaryDir &tmp, const QString &name)
{
    QDir(tmp.path()).mkpath(name);
    return QDir(tmp.path()).absoluteFilePath(name);
}
void put(const QString &dir, const QString &name, const QByteArray &data = "x")
{
    QFile f(QDir(dir).absoluteFilePath(name));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
QByteArray read(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
QStringList names(const QStringList &paths)
{
    QStringList n;
    for (const QString &p : paths) n << QFileInfo(p).fileName();
    n.sort();
    return n;
}
}  // namespace

void tst_fileops::sidecarPathByFormat()
{
/*
    Lightroom writes XMP INTO JPEG/TIFF/PNG/DNG and ignores a sidecar beside them, so
    those get Winnow's own full-name sidecar; raw and HEIC keep Lightroom's name.
*/
    const QString d = sub(tmp, "naming");
    auto sc = [&](const QString &n) {
        return QFileInfo(FileOps::sidecarPath(QDir(d).absoluteFilePath(n))).fileName();
    };
    QCOMPARE(sc("IMG_1.NEF"), QString("IMG_1.xmp"));
    QCOMPARE(sc("IMG_1.HEIC"), QString("IMG_1.xmp"));
    QCOMPARE(sc("IMG_1.JPG"), QString("IMG_1.JPG.xmp"));
    QCOMPARE(sc("IMG_1.tif"), QString("IMG_1.tif.xmp"));
    QCOMPARE(sc("IMG_1.png"), QString("IMG_1.png.xmp"));
    QCOMPARE(sc("IMG_1.dng"), QString("IMG_1.dng.xmp"));
    QCOMPARE(sc("a.b.NEF"), QString("a.b.xmp"));        // Lightroom's rule: last dot
}

void tst_fileops::legacySidecarReadWhenAlone()
{
/*
    Winnow used to write IMG_1.xmp for a JPEG. With no other file claiming the name that
    is still the JPEG's, so the edits it holds keep showing.
*/
    const QString d = sub(tmp, "legacyAlone");
    put(d, "IMG_1.JPG");
    put(d, "IMG_1.xmp", "old");
    const QString jpg = QDir(d).absoluteFilePath("IMG_1.JPG");
    QCOMPARE(QFileInfo(FileOps::existingSidecar(jpg)).fileName(), QString("IMG_1.xmp"));

    // once the full-name one exists it wins
    put(d, "IMG_1.JPG.xmp", "new");
    QCOMPARE(QFileInfo(FileOps::existingSidecar(jpg)).fileName(), QString("IMG_1.JPG.xmp"));
}

void tst_fileops::legacySidecarBelongsToTheRaw()
{
/*
    In a raw+JPEG pair IMG_1.xmp is the raw's Lightroom sidecar. The JPEG must not read
    it -- that was the defect: the raw's keywords and rating showing on the JPEG.
*/
    const QString d = sub(tmp, "legacyRaw");
    put(d, "IMG_1.NEF");
    put(d, "IMG_1.JPG");
    put(d, "IMG_1.xmp");
    QVERIFY(FileOps::existingSidecar(QDir(d).absoluteFilePath("IMG_1.JPG")).isEmpty());
    QCOMPARE(QFileInfo(FileOps::existingSidecar(QDir(d).absoluteFilePath("IMG_1.NEF")))
                 .fileName(), QString("IMG_1.xmp"));
}

void tst_fileops::prepareMovesLegacyWhenAlone()
{
    const QString d = sub(tmp, "prepareAlone");
    put(d, "IMG_1.JPG");
    put(d, "IMG_1.xmp", "recipe");
    const QString path = FileOps::prepareSidecarForWrite(QDir(d).absoluteFilePath("IMG_1.JPG"));
    QCOMPARE(QFileInfo(path).fileName(), QString("IMG_1.JPG.xmp"));
    QCOMPARE(read(path), QByteArray("recipe"));
    QVERIFY(!QFile::exists(QDir(d).absoluteFilePath("IMG_1.xmp")));
}

void tst_fileops::prepareCopiesLegacySharedByFullNameImages()
{
/*
    A DNG+JPG pair both read the old IMG_1.xmp (neither is a raw owner). Moving it to
    the JPEG's name would strip the DNG's edits, so the first writer copies.
*/
    const QString d = sub(tmp, "prepareShared");
    put(d, "IMG_1.DNG");
    put(d, "IMG_1.JPG");
    put(d, "IMG_1.xmp", "recipe");
    const QString path = FileOps::prepareSidecarForWrite(QDir(d).absoluteFilePath("IMG_1.JPG"));
    QCOMPARE(read(path), QByteArray("recipe"));
    QCOMPARE(read(QDir(d).absoluteFilePath("IMG_1.xmp")), QByteArray("recipe"));
}

void tst_fileops::prepareLeavesTheRawsSidecarAlone()
{
    const QString d = sub(tmp, "prepareRaw");
    put(d, "IMG_1.NEF");
    put(d, "IMG_1.JPG");
    put(d, "IMG_1.xmp", "lightroom");
    const QString path = FileOps::prepareSidecarForWrite(QDir(d).absoluteFilePath("IMG_1.JPG"));
    QCOMPARE(QFileInfo(path).fileName(), QString("IMG_1.JPG.xmp"));
    QVERIFY(!QFile::exists(path));              // nothing carried: a fresh document
    QCOMPARE(read(QDir(d).absoluteFilePath("IMG_1.xmp")), QByteArray("lightroom"));
}

void tst_fileops::companionsSplitARawJpegPair()
{
    const QString d = sub(tmp, "pairCompanions");
    put(d, "IMG_1.NEF");
    put(d, "IMG_1.JPG");
    put(d, "IMG_1.xmp");
    put(d, "IMG_1.JPG.xmp");
    QCOMPARE(names(FileOps::companions(QDir(d).absoluteFilePath("IMG_1.NEF"))),
             QStringList{"IMG_1.xmp"});
    QCOMPARE(names(FileOps::companions(QDir(d).absoluteFilePath("IMG_1.JPG"))),
             QStringList{"IMG_1.JPG.xmp"});
}

void tst_fileops::trashingTheJpegKeepsTheRawsSidecar()
{
/*
    Before full-name sidecars, trashing the JPEG of a pair trashed IMG_1.xmp with it --
    the NEF's Lightroom edits.
*/
    useFakeTrash();
    const QString d = sub(tmp, "pairTrash");
    put(d, "IMG_1.NEF");
    put(d, "IMG_1.JPG");
    put(d, "IMG_1.xmp");
    put(d, "IMG_1.JPG.xmp");
    const auto r = FileOps::trashFiles({QDir(d).absoluteFilePath("IMG_1.JPG")});
    QCOMPARE(r.trashed.size(), 1);
    QCOMPARE(QDir(d).entryList(QDir::Files, QDir::Name),
             (QStringList{"IMG_1.NEF", "IMG_1.xmp"}));
}

void tst_fileops::moveRenamesFullNameSidecar()
{
/*
    A renaming move must keep a full-name sidecar full-name; DSC_1.xmp would be the name
    of a raw beside it.
*/
    const QString d = sub(tmp, "moveFull");
    QDir(d).mkdir("dest");
    put(d, "DSC_1.JPG", "image");
    put(d, "DSC_1.JPG.xmp", "recipe");
    const QString dst = QDir(d).absoluteFilePath("dest/Sunset.JPG");
    QVERIFY(FileOps::moveFile(QDir(d).absoluteFilePath("DSC_1.JPG"), dst));
    QCOMPARE(read(QDir(d).absoluteFilePath("dest/Sunset.JPG.xmp")), QByteArray("recipe"));
    QVERIFY(!QFile::exists(QDir(d).absoluteFilePath("dest/Sunset.xmp")));
}

QTEST_MAIN(tst_fileops)
#include "tst_fileops.moc"
