/*
    Unit tests for Main/devpreviewpolicy.h -- the rule deciding where the BACKGROUND
    devPreview build may run.

    Why this rule is worth its own test: it is the only thing preventing a camera card
    from queueing hours of sensor decoding whose output is orphaned the moment the files
    are ingested to a different path. Both halves of it are the kind that fail quietly --
    a prefix test that forgets the separator silently swallows a sibling folder, and a
    removable-media test that is really "is it under /Volumes" silently excludes an
    external library drive.

    Pure logic, no MainWindow and no media: the scope half is exercised directly, and the
    volume half only where the answer is knowable on any machine (the boot volume, and
    the Unknown-means-proceed fallback).
*/

#include <QtTest>
#include <QDir>
#include <QUuid>

#include "Main/devpreviewpolicy.h"

class TstDevPreviewPolicy : public QObject
{
    Q_OBJECT
private slots:
    void inScopeFolderIsEligible();
    void newSubfolderIsEligibleWithoutAScan();
    void siblingSharingAPrefixIsNotSwallowed();
    void outOfScopeFolderIsSkipped();
    void excludedBranchIsSkipped();
    void nonRecursiveIncludeDoesNotReachDown();
    void emptyScopeFallsBackToTheVolume();
    void emptyFolderIsNeverEligible();
    void unknownVolumeIsTreatedAsLocal();
    // which images are worth a speculative build (autoDevPreviews, 2026-11-28)
    void sonyArwPreviewIsSmallOnA4kScreen();
    void olympusPreviewIsGoodEnough();
    void loupeNeedIsCappedBySensor();
    void missingPreviewCountsAsSmall();
    void harvestFormatsAreSlowNonRaws();
    void slowDecodeNeedsTimeAndRatio();
    void nearlyFullAtNinetyPercent();
    void suggestCapFitsTheDisk();
    void nearestFirstTiesGoForward();
};

namespace {
CatalogScope scopeOf(std::initializer_list<CatalogScopeEntry> rows)
{
    CatalogScope s;
    for (const auto &r : rows) s << r;
    return s;
}
const CatalogScopeEntry kPictures{"/Users/x/Pictures", true, true};
}  // namespace

void TstDevPreviewPolicy::inScopeFolderIsEligible()
{
    const CatalogScope s = scopeOf({kPictures});
    QVERIFY(DevPreviewPolicy::folderEligible(s, "/Users/x/Pictures"));
    QVERIFY(DevPreviewPolicy::folderEligible(s, "/Users/x/Pictures/2024/June"));
}

void TstDevPreviewPolicy::newSubfolderIsEligibleWithoutAScan()
{
    /*  The point of testing scope MEMBERSHIP rather than catalog ROWS: a folder created
        a minute ago holds no catalog rows at all, and must still build previews. */
    const CatalogScope s = scopeOf({kPictures});
    QVERIFY(DevPreviewPolicy::folderEligible(s, "/Users/x/Pictures/2026-09-12 Brand New"));
}

void TstDevPreviewPolicy::siblingSharingAPrefixIsNotSwallowed()
{
    /*  "/Users/x/Pictures" must not admit "/Users/x/PicturesOld" -- a plain startsWith
        with no separator would, and the mistake is invisible until a folder nobody
        nominated starts rendering. */
    const CatalogScope s = scopeOf({kPictures});
    QVERIFY(!DevPreviewPolicy::folderEligible(s, "/Users/x/PicturesOld"));
    QVERIFY(!DevPreviewPolicy::folderEligible(s, "/Users/x/PicturesOld/2024"));
}

void TstDevPreviewPolicy::outOfScopeFolderIsSkipped()
{
    /*  The case the rule exists for. A card is never under the library root, so it is
        skipped whatever kind of volume it turns out to be. */
    const CatalogScope s = scopeOf({kPictures});
    QVERIFY(!DevPreviewPolicy::folderEligible(s, "/Volumes/Untitled/DCIM/100MSDCF"));
    QVERIFY(!DevPreviewPolicy::folderEligible(s, "/Users/x/Downloads"));
}

void TstDevPreviewPolicy::excludedBranchIsSkipped()
{
    const CatalogScope s = scopeOf({kPictures,
                                    {"/Users/x/Pictures/Scratch", false, true}});
    QVERIFY(DevPreviewPolicy::folderEligible(s, "/Users/x/Pictures/2024"));
    QVERIFY(!DevPreviewPolicy::folderEligible(s, "/Users/x/Pictures/Scratch"));
    QVERIFY(!DevPreviewPolicy::folderEligible(s, "/Users/x/Pictures/Scratch/tmp"));
}

void TstDevPreviewPolicy::nonRecursiveIncludeDoesNotReachDown()
{
    const CatalogScope s = scopeOf({{"/Users/x/Pictures", true, false}});
    QVERIFY(DevPreviewPolicy::folderEligible(s, "/Users/x/Pictures"));
    QVERIFY(!DevPreviewPolicy::folderEligible(s, "/Users/x/Pictures/2024"));
}

void TstDevPreviewPolicy::emptyScopeFallsBackToTheVolume()
{
    /*  No scope table: the user has not said where their library is, so the preference
        must still work on local disk. The test directory is on whatever volume the
        source tree lives on, which is by construction not removable. */
    const CatalogScope none;
    QVERIFY(DevPreviewPolicy::folderEligible(none, QDir::homePath()));
    QVERIFY(DevPreviewPolicy::folderEligible(none, QDir::rootPath()));
}

void TstDevPreviewPolicy::emptyFolderIsNeverEligible()
{
    QVERIFY(!DevPreviewPolicy::folderEligible(scopeOf({kPictures}), QString()));
    QVERIFY(!DevPreviewPolicy::folderEligible(CatalogScope(), QString()));
}

void TstDevPreviewPolicy::unknownVolumeIsTreatedAsLocal()
{
    /*  A path that cannot be resolved must not silently withhold work that happened
        before this rule existed: Unknown counts as local fixed. */
    const QString nowhere = "/no/such/path/anywhere/" + QUuid::createUuid().toString();
    QCOMPARE(VolumeInfo::kindOf(nowhere), VolumeInfo::Kind::Unknown);
    QVERIFY(VolumeInfo::isLocalFixed(nowhere));
    QVERIFY(DevPreviewPolicy::folderEligible(CatalogScope(), nowhere));
}

void TstDevPreviewPolicy::sonyArwPreviewIsSmallOnA4kScreen()
{
    /*  The case the rule exists for: a Sony A9 II ARW embeds 1616x1080 against a
        6048x4024 sensor, shown on a 3840 px screen. Browse would show it upscaled. */
    QVERIFY(DevPreviewPolicy::smallPreviewRaw(1616, 1080, 6048, 4024, 3840));
    // the A1 II: same preview, 8672 px sensor
    QVERIFY(DevPreviewPolicy::smallPreviewRaw(1616, 1080, 8672, 5784, 3840));
}

void TstDevPreviewPolicy::olympusPreviewIsGoodEnough()
{
    /*  An OM-1 embeds 3200 px against a 5220 px sensor: 0.83 of a 3840 px screen, above
        kSmallPreviewFraction, so Browse already shows it well and nothing is built. */
    QVERIFY(!DevPreviewPolicy::smallPreviewRaw(3200, 2400, 5220, 3912, 3840));
    // a full-size embedded JPEG is never small
    QVERIFY(!DevPreviewPolicy::smallPreviewRaw(6048, 4024, 6048, 4024, 3840));
}

void TstDevPreviewPolicy::loupeNeedIsCappedBySensor()
{
    /*  A 12 MP raw on an 8K screen needs only its own size: a full-size preview of it
        is not "small" just because the screen is bigger. */
    QCOMPARE(DevPreviewPolicy::loupeNeed(4256, 2832, 7680), 4256);
    QCOMPARE(DevPreviewPolicy::loupeNeed(6048, 4024, 3840), 3840);
    QCOMPARE(DevPreviewPolicy::loupeNeed(6048, 4024, 0), 6048);      // no screen known
    QVERIFY(!DevPreviewPolicy::smallPreviewRaw(4256, 2832, 4256, 2832, 7680));
}

void TstDevPreviewPolicy::missingPreviewCountsAsSmall()
{
    QVERIFY(DevPreviewPolicy::smallPreviewRaw(0, 0, 6048, 4024, 3840));
}

void TstDevPreviewPolicy::harvestFormatsAreSlowNonRaws()
{
    /*  Only formats the decoder color manages (Metadata::iccFormats) and that can be
        slow; a JPEG of a JPEG saves nothing. */
    QVERIFY(DevPreviewPolicy::sourceRenderFormat("tif"));
    QVERIFY(DevPreviewPolicy::sourceRenderFormat("png"));
    QVERIFY(!DevPreviewPolicy::sourceRenderFormat("jpg"));
    QVERIFY(!DevPreviewPolicy::sourceRenderFormat("arw"));
}

void TstDevPreviewPolicy::slowDecodeNeedsTimeAndRatio()
{
    /*  The 4872x3248 16-bit ProPhoto TIFF that prompted this: over a second against
        ~127 ms for a JPEG of the same size. */
    QVERIFY(DevPreviewPolicy::slowDecode(1100, 4872, 3248));
    // under the floor, however slow per pixel
    QVERIFY(!DevPreviewPolicy::slowDecode(300, 1000, 1000));
    /*  A 100 MP file at 900 ms is about what its JPEG would take (~800 ms): a cached
        JPEG would save nothing. */
    QVERIFY(!DevPreviewPolicy::slowDecode(900, 12000, 8333));
    QVERIFY(DevPreviewPolicy::slowDecode(2600, 12000, 8333));
}

void TstDevPreviewPolicy::nearlyFullAtNinetyPercent()
{
    constexpr qint64 GB = 1024LL * 1024 * 1024;
    QVERIFY(!DevPreviewPolicy::nearlyFull(17 * GB, 20 * GB));
    QVERIFY(DevPreviewPolicy::nearlyFull(18 * GB, 20 * GB));
    QVERIFY(DevPreviewPolicy::nearlyFull(0, 0));             // no cache = no room
}

void TstDevPreviewPolicy::suggestCapFitsTheDisk()
{
    constexpr qint64 GB = 1024LL * 1024 * 1024;
    QList<qint64> steps;
    for (qint64 s : {5, 10, 20, 50, 100, 200}) steps << s * GB;
    // 19 GB used of 20, 1.5 TB free: the next step up
    QCOMPARE(DevPreviewPolicy::suggestCap(19 * GB, 20 * GB, 1500 * GB, steps), 50 * GB);
    // 19 GB used, 60 GB free: 19 + 15 = 34 GB ceiling, so 50 does not fit
    QCOMPARE(DevPreviewPolicy::suggestCap(19 * GB, 20 * GB, 60 * GB, steps), 0LL);
    // already at the largest step
    QCOMPARE(DevPreviewPolicy::suggestCap(190 * GB, 200 * GB, 5000 * GB, steps), 0LL);
}

void TstDevPreviewPolicy::nearestFirstTiesGoForward()
{
    /*  Current row 10: 10 itself is left out (its picture is on screen), then 11 before
        9 (ahead first at equal distance), then 12, 8, ... */
    const QList<int> rows {5, 6, 7, 8, 9, 10, 11, 12, 13};
    const QList<int> expect {11, 9, 12, 8, 13, 7, 6, 5};
    QCOMPARE(DevPreviewPolicy::nearestFirst(rows, 10), expect);
    // a current row off screen leaves every row in, nearest first
    QCOMPARE(DevPreviewPolicy::nearestFirst({1, 2, 3}, 0), QList<int>({1, 2, 3}));
}

QTEST_GUILESS_MAIN(TstDevPreviewPolicy)
#include "tst_devpreviewpolicy.moc"
