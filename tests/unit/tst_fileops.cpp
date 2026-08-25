#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "Utilities/fileops.h"
#include "Cache/developpreviewcache.h"

/*
    FileOps -- the single choke point every image file operation goes through.

    An image in Winnow is the image PLUS its sidecars, and the sidecar holds the entire
    Develop recipe. Before FileOps that pairing was reimplemented four different ways and
    they disagreed; these tests pin the one definition.

    trashFile is deliberately not exercised: it would put files in the user's real Trash.
*/
class tst_fileops : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void companionsFindsXmpAndTxt();
    void companionsIsCaseInsensitive();
    void companionsExcludesPairedImages();
    void copyCarriesCompanions();
    void copyRenamesCompanionsToDestinationBase();
    void moveCarriesCompanionsAndPreview();

private:
    QString p(const QString &name) const;
    void touch(const QString &name, const QByteArray &data = "x") const;

    QTemporaryDir tmp;
    QTemporaryDir cacheTmp;
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

void tst_fileops::init()
{
    QVERIFY(tmp.isValid());
    QVERIFY(cacheTmp.isValid());

    // start each test from an empty folder
    QDir d(tmp.path());
    for (const QString &f : d.entryList(QDir::Files | QDir::Hidden)) d.remove(f);

    DevelopPreviewCache &c = DevelopPreviewCache::instance();
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

    DevelopPreviewCache &c = DevelopPreviewCache::instance();
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

QTEST_MAIN(tst_fileops)
#include "tst_fileops.moc"
