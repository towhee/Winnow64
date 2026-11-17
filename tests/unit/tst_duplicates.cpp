#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtEndian>

#include "Cache/cachedb.h"
#include "Cache/catalog.h"
#include "Cache/devpreviewcache.h"
#include "Cache/duplicateclassify.h"
#include "Cache/pathkey.h"
#include "ImageFormats/Video/mov.h"

/*
    LIBRARY DUPLICATES (Cache/duplicates.h): the rule (classifyDuplicates, pure), the
    index it is fed from (candidates, fingerprints, the hooks that keep it current) and
    the video fingerprint (MOV::durationMs). The decode-and-hash step is not here: it
    needs real images and Metadata's parsers, and it is a SHA-1 of what Qt decodes.
*/
class tst_duplicates : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    // the rule
    void aPairIsTwoDuplicates();
    void okayingEitherOfAPairClearsBoth();
    void aTripleWithOneOkayedKeepsTwoRed();
    void sameHashDifferentTypeOrTimeIsNotADuplicate();
    void noFingerprintIsInNoGroup();
    void videosMatchOnDuration();

    // the video fingerprint
    void durationFromMvhdVersion0();
    void durationFromMvhdVersion1AfterA64BitMdat();
    void noMoovIsNoDuration();

    // the index
    void candidatesShareTheWholeKey();
    void candidatesForAKeyIncludeItsMates();
    void aContentChangeClearsTheFingerprintASidecarChangeDoesNot();
    void theHooksHearInsertsAndDeletes();
    void schema32IsRerunnable();

private:
    DuplicateRow row(const QString &path, qint64 captured, const QString &ext,
                     const QByteArray &hash, qint64 durationMs = DuplicateRow::kNoDuration);
    CatalogRow catalogRow(const QString &name, qint64 captured, int w = 6000, int h = 4000);
    QTemporaryDir tmp;
    QTemporaryDir cacheTmp;
};

namespace {
QByteArray be32(quint32 v)
{
    QByteArray b(4, '\0');
    qToBigEndian(v, b.data());
    return b;
}
QByteArray be64(quint64 v)
{
    QByteArray b(8, '\0');
    qToBigEndian(v, b.data());
    return b;
}
QByteArray box(const char *type, const QByteArray &payload)
{
    return be32(quint32(8 + payload.size())) + QByteArray(type, 4) + payload;
}
QByteArray mvhd0(quint32 timescale, quint32 duration)
{
    QByteArray p;
    p += QByteArray(4, '\0');                   // version 0, flags
    p += be32(0) + be32(0);                     // ctime, mtime
    p += be32(timescale) + be32(duration);
    p += QByteArray(80, '\0');                  // the rest of mvhd
    return box("mvhd", p);
}
QByteArray mvhd1(quint32 timescale, quint64 duration)
{
    QByteArray p;
    p += QByteArray(1, '\1') + QByteArray(3, '\0');
    p += be64(0) + be64(0);
    p += be32(timescale) + be64(duration);
    p += QByteArray(80, '\0');
    return box("mvhd", p);
}
qint64 durationOf(const QByteArray &file)
{
    QBuffer b;
    b.setData(file);
    b.open(QIODevice::ReadOnly);
    return MOV::durationMs(b);
}
const QByteArray kHashA = QByteArray(20, 'a');
const QByteArray kHashB = QByteArray(20, 'b');
}   // namespace

DuplicateRow tst_duplicates::row(const QString &path, qint64 captured, const QString &ext,
                                 const QByteArray &hash, qint64 durationMs)
{
    DuplicateRow r;
    r.path = path;
    r.pathkey = cachePathKey(path);
    r.captured = captured;
    r.ext = ext;
    r.pixelHash = hash;
    r.durationMs = durationMs;
    return r;
}

CatalogRow tst_duplicates::catalogRow(const QString &name, qint64 captured, int w, int h)
{
    const QString p = QDir(tmp.path()).absoluteFilePath(name);
    if (!QFile::exists(p)) {
        QFile f(p);
        if (f.open(QIODevice::WriteOnly)) f.write("not really an image");
    }
    const QFileInfo fi(p);
    CatalogRow r;
    r.path = p;
    r.folder = fi.absoluteDir().path();
    r.filename = fi.fileName();
    r.ext = fi.suffix().toLower();
    r.srcSize = fi.size();
    r.srcMtime = fi.lastModified().toSecsSinceEpoch();
    r.captured = QDateTime::fromSecsSinceEpoch(captured, QTimeZone::UTC);
    r.width = w;
    r.height = h;
    return r;
}

void tst_duplicates::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(tmp.isValid());
    QVERIFY(cacheTmp.isValid());
}

void tst_duplicates::cleanupTestCase()
{
    Catalog::instance().setContentChangedHook(nullptr);
    Catalog::instance().setMembershipChangedHook(nullptr);
    CacheDb::instance().closeThisThread();
}

void tst_duplicates::init()
{
    DevPreviewCache &c = DevPreviewCache::instance();
    c.setCacheDir(cacheTmp.path());
    Catalog::instance().setContentChangedHook(nullptr);
    Catalog::instance().setMembershipChangedHook(nullptr);
    Catalog::instance().clear();
}

/* ---------------------------------------------------------------- the rule */

void tst_duplicates::aPairIsTwoDuplicates()
{
    const QVector<DuplicateRow> rows = {row("/a/1.arw", 100, "arw", kHashA),
                                        row("/b/1.arw", 100, "arw", kHashA)};
    DuplicateCounts c;
    const auto st = classifyDuplicates(rows, {}, &c);
    QCOMPARE(st.size(), 2);
    QCOMPARE(st.value("/a/1.arw").status, DuplicateStatus::Duplicate);
    QCOMPARE(st.value("/b/1.arw").status, DuplicateStatus::Duplicate);
    QCOMPARE(st.value("/a/1.arw").twins, 1);
    QVERIFY(!st.value("/a/1.arw").video);
    QCOMPARE(c.found, 2);
    QCOMPARE(c.groups, 1);
    QCOMPARE(c.okayed, 0);
}

void tst_duplicates::okayingEitherOfAPairClearsBoth()
{
    const QVector<DuplicateRow> rows = {row("/a/1.arw", 100, "arw", kHashA),
                                        row("/b/1.arw", 100, "arw", kHashA)};
    DuplicateCounts c;
    const auto st = classifyDuplicates(rows, {"/b/1.arw"}, &c);
    QCOMPARE(st.value("/a/1.arw").status, DuplicateStatus::Okayed);
    QCOMPARE(st.value("/b/1.arw").status, DuplicateStatus::Okayed);
    QCOMPARE(c.found, 0);
    QCOMPARE(c.okayed, 2);
    QCOMPARE(c.groups, 0);
}

void tst_duplicates::aTripleWithOneOkayedKeepsTwoRed()
{
    const QVector<DuplicateRow> rows = {row("/a/1.jpg", 100, "jpg", kHashA),
                                        row("/b/1.jpg", 100, "jpg", kHashA),
                                        row("/c/1.jpg", 100, "jpg", kHashA)};
    DuplicateCounts c;
    const auto st = classifyDuplicates(rows, {"/a/1.jpg"}, &c);
    QCOMPARE(st.value("/a/1.jpg").status, DuplicateStatus::Okayed);
    QCOMPARE(st.value("/b/1.jpg").status, DuplicateStatus::Duplicate);
    QCOMPARE(st.value("/c/1.jpg").status, DuplicateStatus::Duplicate);
    QCOMPARE(st.value("/b/1.jpg").twins, 2);
    QCOMPARE(c.found, 2);
    QCOMPARE(c.okayed, 1);
}

void tst_duplicates::sameHashDifferentTypeOrTimeIsNotADuplicate()
{
    /*  A raw+JPEG pair shares the capture time and is not a duplicate; neither are two
        frames of a burst, which differ in their pixels even within one second -- and
        should two hashes ever agree across a different second, the time still splits
        them. */
    const QVector<DuplicateRow> rows = {row("/a/1.arw", 100, "arw", kHashA),
                                        row("/a/1.jpg", 100, "jpg", kHashA),
                                        row("/a/2.arw", 101, "arw", kHashA),
                                        row("/a/3.arw", 100, "arw", kHashB)};
    DuplicateCounts c;
    QVERIFY(classifyDuplicates(rows, {}, &c).isEmpty());
    QCOMPARE(c.found, 0);
}

void tst_duplicates::noFingerprintIsInNoGroup()
{
    const QVector<DuplicateRow> rows = {
        row("/a/1.arw", 100, "arw", QByteArray()),
        row("/b/1.arw", 100, "arw", QByteArray()),
        row("/c/1.arw", 100, "arw", DuplicateStatus::failedHash()),
        row("/d/1.arw", 100, "arw", DuplicateStatus::failedHash()),
        row("/a/1.avi", 100, "avi", QByteArray(), -1),
        row("/b/1.avi", 100, "avi", QByteArray(), -1)};
    QVERIFY(classifyDuplicates(rows, {}).isEmpty());
}

void tst_duplicates::videosMatchOnDuration()
{
    const QVector<DuplicateRow> rows = {row("/a/1.mov", 100, "mov", QByteArray(), 12345),
                                        row("/b/1.mov", 100, "mov", QByteArray(), 12345),
                                        row("/c/2.mov", 100, "mov", QByteArray(), 12346)};
    const auto st = classifyDuplicates(rows, {});
    QCOMPARE(st.size(), 2);
    QCOMPARE(st.value("/a/1.mov").status, DuplicateStatus::Duplicate);
    QVERIFY(st.value("/a/1.mov").video);
    QVERIFY(!st.contains("/c/2.mov"));
}

/* ------------------------------------------------------- the video fingerprint */

void tst_duplicates::durationFromMvhdVersion0()
{
    const QByteArray file = box("ftyp", QByteArray("qt  \0\0\0\0qt  ", 12))
                            + box("moov", mvhd0(600, 600 * 12 + 300));    // 12.5 s
    QCOMPARE(durationOf(file), qint64(12500));
}

void tst_duplicates::durationFromMvhdVersion1AfterA64BitMdat()
{
    /*  Camera files put the media first, and past 4 GB its box carries a 64-bit size
        (size field 1). A walker that read only the 32-bit size stopped there. */
    const QByteArray media(64, 'x');
    const QByteArray mdat = be32(1) + QByteArray("mdat", 4) + be64(16 + media.size()) + media;
    const QByteArray file = box("ftyp", QByteArray("isom\0\0\0\0isom", 12)) + mdat
                            + box("moov", box("udta", QByteArray(8, '\0'))
                                              + mvhd1(90000, 90000ull * 61));
    QCOMPARE(durationOf(file), qint64(61000));
}

void tst_duplicates::noMoovIsNoDuration()
{
    QCOMPARE(durationOf(box("ftyp", QByteArray("isom\0\0\0\0isom", 12)) + box("mdat", "abc")), qint64(-1));
    QCOMPARE(durationOf(QByteArray("RIFF....AVI LIST")), qint64(-1));
    QCOMPARE(durationOf(box("moov", mvhd0(0, 100))), qint64(-1));     // no timescale
}

/* ------------------------------------------------------------------ the index */

void tst_duplicates::candidatesShareTheWholeKey()
{
    Catalog &cat = Catalog::instance();
    QCOMPARE(cat.commit({catalogRow("a1.arw", 1000), catalogRow("a2.arw", 1000),
                         catalogRow("b1.arw", 1000, 4000, 6000),   // portrait: not a mate
                         catalogRow("c1.jpg", 1000),               // other type
                         catalogRow("d1.arw", 2000)}), 5);         // other second
    const QVector<DuplicateRow> c = cat.duplicateCandidates();
    QStringList names;
    for (const DuplicateRow &r : c) names << QFileInfo(r.path).fileName();
    names.sort();
    QCOMPARE(names, QStringList({"a1.arw", "a2.arw"}));
    QVERIFY(c.first().pixelHash.isEmpty());
    QCOMPARE(c.first().durationMs, DuplicateRow::kNoDuration);
}

void tst_duplicates::candidatesForAKeyIncludeItsMates()
{
    Catalog &cat = Catalog::instance();
    const CatalogRow a = catalogRow("m1.arw", 3000), b = catalogRow("m2.arw", 3000),
                     lone = catalogRow("lone.arw", 4000);
    QCOMPARE(cat.commit({a, b, lone}), 3);
    QCOMPARE(cat.duplicateCandidatesFor({cachePathKey(a.path)}).size(), 2);
    QVERIFY(cat.duplicateCandidatesFor({cachePathKey(lone.path)}).isEmpty());
    /* Asking for both of a group reads it once. */
    QCOMPARE(cat.duplicateCandidatesFor({cachePathKey(a.path), cachePathKey(b.path)}).size(),
             2);
}

void tst_duplicates::aContentChangeClearsTheFingerprintASidecarChangeDoesNot()
{
    Catalog &cat = Catalog::instance();
    CatalogRow a = catalogRow("f1.arw", 5000), b = catalogRow("f2.arw", 5000);
    QCOMPARE(cat.commit({a, b}), 2);
    QVector<DuplicateRow> c = cat.duplicateCandidates();
    QCOMPARE(c.size(), 2);
    for (DuplicateRow &r : c) r.pixelHash = kHashA;
    QCOMPARE(cat.setDuplicateFingerprints(c), 2);
    QCOMPARE(cat.duplicateFingerprinted().size(), 2);
    QCOMPARE(classifyDuplicates(cat.duplicateFingerprinted(), {}).size(), 2);

    /* A sidecar edit rewrites the row and keeps the hash: the pixels did not change. */
    a.sidecarMtime = 42;
    QCOMPARE(cat.commit({a}), 1);
    QCOMPARE(cat.duplicateFingerprinted().size(), 2);

    /* New content clears it: the file is fingerprinted again. */
    b.srcMtime += 10;
    QCOMPARE(cat.commit({b}), 1);
    const QVector<DuplicateRow> after = cat.duplicateFingerprinted();
    QCOMPARE(after.size(), 1);
    QCOMPARE(after.first().path, a.path);

    /* A fingerprint taken against stamps the row no longer has is not stored. */
    QVector<DuplicateRow> stale = cat.duplicateCandidatesFor({cachePathKey(b.path)});
    for (DuplicateRow &r : stale) {
        r.pixelHash = kHashB;
        if (r.path == b.path) r.srcMtime -= 10;         // as read before the change
    }
    cat.setDuplicateFingerprints(stale);
    for (const DuplicateRow &r : cat.duplicateFingerprinted())
        QVERIFY2(r.path != b.path, "a stale fingerprint was stored");
}

void tst_duplicates::theHooksHearInsertsAndDeletes()
{
    Catalog &cat = Catalog::instance();
    QStringList content;
    int membership = 0;
    cat.setContentChangedHook([&](const QStringList &keys) { content << keys; });
    cat.setMembershipChangedHook([&] { ++membership; });

    const CatalogRow a = catalogRow("h1.arw", 6000);
    QCOMPARE(cat.commit({a}), 1);
    QCOMPARE(content, QStringList({cachePathKey(a.path)}));

    /* Unchanged: skipped by commit, so nothing to hear. */
    content.clear();
    QCOMPARE(cat.commit({a}), 0);
    QVERIFY(content.isEmpty());

    /* A sidecar-only rewrite is not new content. */
    CatalogRow side = a;
    side.sidecarMtime = 7;
    QCOMPARE(cat.commit({side}), 1);
    QVERIFY(content.isEmpty());

    cat.onDeleted(a.path);
    QCOMPARE(membership, 1);

    cat.setContentChangedHook(nullptr);
    cat.setMembershipChangedHook(nullptr);
}

void tst_duplicates::schema32IsRerunnable()
{
    /*  Every block from 16 on must survive a rewound user_version on a current file --
        the columns and both indexes already exist. */
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = QDir(dir.path()).absoluteFilePath("index.db");
    CacheDb::instance().setPath(dbPath);
    Catalog &cat = Catalog::instance();
    QVERIFY(cat.isAvailable());
    QCOMPARE(cat.commit({catalogRow("r1.arw", 7000), catalogRow("r2.arw", 7000)}), 2);
    {
        QSqlQuery q(CacheDb::instance().db());
        QVERIFY(q.exec("PRAGMA user_version = 31"));
    }
    CacheDb::instance().closeThisThread();
    CacheDb::instance().setPath(QDir(dir.path()).absoluteFilePath("other.db"));
    CacheDb::instance().setPath(dbPath);
    QVERIFY2(cat.isAvailable(), "a rewound version 31 file must migrate, not be moved aside");
    QCOMPARE(cat.duplicateCandidates().size(), 2);
    {
        QSqlQuery q(CacheDb::instance().db());
        QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE type = 'index'"
                       " AND name IN ('image_dupkey', 'image_fingerprinted')"));
        int n = 0;
        while (q.next()) ++n;
        QCOMPARE(n, 2);
    }
    CacheDb::instance().closeThisThread();
    CacheDb::instance().setPath(QDir(cacheTmp.path()).absoluteFilePath("index.db"));
}

QTEST_MAIN(tst_duplicates)
#include "tst_duplicates.moc"
