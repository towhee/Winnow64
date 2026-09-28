// Unit tests for Utilities/versionkey.h -- the row key of a version (virtual copy).
//
// The key must round-trip to its source file, must never be mistaken for a real path,
// must hash through cachePathKey without losing the suffix, and must sort immediately
// after its master and before every other file.

#include <QtTest>
#include "Utilities/versionkey.h"
#include "Cache/pathkey.h"

class TstVersionKey : public QObject
{
    Q_OBJECT
private slots:
    void masterIsUnchanged();
    void roundTrip();
    void windowsDrivePath();
    void notAVersion_data();
    void notAVersion();
    void cachePathKeyKeepsSuffix();
    void sortsAfterMasterBeforeOthers();
    void sortSuffixOrdersIds();
};

void TstVersionKey::masterIsUnchanged()
{
    const QString p = "/Photos/IMG_1.CR3";
    QCOMPARE(VersionKey::make(p, 0), p);
    QVERIFY(!VersionKey::isVersion(p));
    QCOMPARE(VersionKey::sourceOf(p), p);
    QCOMPARE(VersionKey::idOf(p), 0);
    QVERIFY(VersionKey::sortSuffix(0).isEmpty());
}

void TstVersionKey::roundTrip()
{
    const QString p = "/Photos/IMG_1.CR3";
    const QString k = VersionKey::make(p, 12);
    QCOMPARE(k, QString("/Photos/IMG_1.CR3/#v12"));
    QVERIFY(VersionKey::isVersion(k));
    QCOMPARE(VersionKey::sourceOf(k), p);
    QCOMPARE(VersionKey::idOf(k), 12);
}

void TstVersionKey::windowsDrivePath()
{
    const QString p = "C:/Users/me/Pictures/DSC_0001.NEF";
    const QString k = VersionKey::make(p, 3);
    QCOMPARE(VersionKey::sourceOf(k), p);
    QCOMPARE(VersionKey::idOf(k), 3);
}

void TstVersionKey::notAVersion_data()
{
    QTest::addColumn<QString>("path");
    // a real folder named "#v2" holding an image
    QTest::newRow("folder #v2") << "/Photos/#v2/IMG_1.JPG";
    // a file whose NAME merely contains the marker
    QTest::newRow("name #v")    << "/Photos/IMG#v2.JPG";
    // separator with no digits, or trailing junk
    QTest::newRow("no digits")  << "/Photos/IMG_1.CR3/#v";
    QTest::newRow("junk")       << "/Photos/IMG_1.CR3/#v2a";
    // the old candidate separators are plain names
    QTest::newRow("pipe")       << "/Photos/IMG_1.CR3|v2";
    QTest::newRow("colons")     << "/Photos/IMG_1.CR3::v2";
    QTest::newRow("empty")      << "";
}

void TstVersionKey::notAVersion()
{
    QFETCH(QString, path);
    QVERIFY(!VersionKey::isVersion(path));
    QCOMPARE(VersionKey::sourceOf(path), path);
    QCOMPARE(VersionKey::idOf(path), 0);
}

void TstVersionKey::cachePathKeyKeepsSuffix()
{
    /* The devpreview cache stores a version under cachePathKey(key); the key must stay
       distinct from the master's and still recover the master's key as its prefix. */
    const QString p = "/Photos/IMG_1.CR3";
    const QString k = VersionKey::make(p, 2);
    const QString ck = cachePathKey(k);
    QVERIFY(ck != cachePathKey(p));
    QVERIFY(ck.startsWith(cachePathKey(p) + "/#v"));
    QCOMPARE(VersionKey::sourceOf(ck), cachePathKey(p));
    QCOMPARE(VersionKey::idOf(ck), 2);
}

void TstVersionKey::sortsAfterMasterBeforeOthers()
{
    /* DataModel::sortKey is the lower-cased SOURCE path plus sortSuffix(id). Composed
       here the same way, since the DataModel cannot be linked into a unit test.
       '-' (0x2D) sorts below '/' (0x2F), so on raw keys "IMG_1.CR3-edit.jpg" would land
       between the master and its versions. The sort key must prevent that. */
    auto sortKey = [](const QString &key) {
        return VersionKey::sourceOf(key).toLower()
               + VersionKey::sortSuffix(VersionKey::idOf(key));
    };
    const QString master = sortKey("/Photos/IMG_1.CR3");
    const QString v1 = sortKey("/Photos/IMG_1.CR3/#v1");
    const QString v2 = sortKey("/Photos/IMG_1.CR3/#v2");
    const QString edit = sortKey("/Photos/IMG_1.CR3-edit.jpg");
    const QString dot = sortKey("/Photos/IMG_1.CR3.bak.jpg");
    const QString next = sortKey("/Photos/IMG_2.CR3");
    QVERIFY(master < v1);
    QVERIFY(v1 < v2);
    QVERIFY(v2 < edit);
    QVERIFY(v2 < dot);
    QVERIFY(v2 < next);
    // raw keys would interleave -- the reason the suffix exists
    QVERIFY(QString("/photos/img_1.cr3-edit.jpg") < QString("/photos/img_1.cr3/#v1"));
}

void TstVersionKey::sortSuffixOrdersIds()
{
    QVERIFY(VersionKey::sortSuffix(2) < VersionKey::sortSuffix(10));
    QVERIFY(VersionKey::sortSuffix(99) < VersionKey::sortSuffix(100));
}

QTEST_GUILESS_MAIN(TstVersionKey)
#include "tst_versionkey.moc"
