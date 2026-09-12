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

QTEST_GUILESS_MAIN(TstDevPreviewPolicy)
#include "tst_devpreviewpolicy.moc"
