#include "Main/mainwindow.h"

#include <cstdlib>          // std::_Exit

/*
    WINNOW_SELFTEST_PARK=1 -- the parked Library self-test (ctest park_restore). See
    DataModel::parkScope and "The Parked Library" in notes/Documentation.txt.

    THE ROUND TRIP A PERSON MAKES: Library, then Folders (with no folder to go back to,
    as after a start-up in the Library), a folder picked, then Library again. The second
    Library must come back RESTORED -- the same rows, the same current image, a model
    that passes verifyIntegrity -- and not by the catalog query and fill it replaces,
    which would pass every other check here while defeating the point. So the restore
    count is asserted too.

    runSelfTest has loaded the fixture folder first; selfTestCatalogueFolder makes it the
    Library's one root and indexes it, and this drives the Module dock's
    Library / Folders buttons (MW::chooseSource), the path the click takes.
*/

namespace {

[[noreturn]] void fail(const QString &step, const QString &why)
{
    fprintf(stderr, "SELFTEST: PARK FAIL [%s] %s\n",
            step.toLocal8Bit().constData(), why.toLocal8Bit().constData());
    fflush(stderr);
    std::_Exit(8);
}

bool waitFor(const std::function<bool()> &done, int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        if (done()) return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
    return done();
}

}  // namespace

bool MW::selfTestCatalogueFolder(const QString &folder)
{
/*
    MAKE THE FIXTURE THE LIBRARY. A folder load only catalogues folders inside the scope
    table, which a fresh test-mode session does not have -- so the Library was empty and
    refused (catalogEmptyOpenManage), and these tests passed only on an index an earlier
    run had left behind. The scope gets the one root, and the scanner indexes it.
*/
    CatalogScopeEntry e;
    e.path = catalogScopeNormalize(folder);
    catalogScope = CatalogScope{e};
    const int files = QDir(folder).entryList(QDir::Files).size();
    if (Catalog::instance().count() < files) startCatalogScan();
    return waitFor([files]{ return Catalog::instance().count() >= files; }, 30000);
}

void MW::selfTestParkLibrary(const QString &folder)
{
    auto settled = [this](G::Scope s) {
        return [this, s]{
            return G::scope == s && dm->rowCount() > 0 && !G::isLoadRunning
                   && !G::isModifyingDatamodel && G::allMetadataAttempted
                   && !loadCurtainUp;
        };
    };
    auto keys = [this]{
        QStringList k;
        for (int r = 0; r < dm->rowCount(); ++r)
            k << dm->index(r, 0).data(G::KeyRole).toString();
        k.sort();
        return k;
    };

    if (!selfTestCatalogueFolder(folder)) fail("catalogue", "the fixture was not indexed");

    // 1. the Library, loaded the ordinary way
    chooseSource(true, "selftest park: Library");
    if (!waitFor(settled(G::Scope::Catalog), 30000))
        fail("first Library", QString("did not settle: rows=%1 scope=%2 loadRunning=%3")
                                  .arg(dm->rowCount()).arg(int(G::scope))
                                  .arg(bool(G::isLoadRunning)));
    if (dm->rowCount() < 2) fail("first Library", "needs at least two rows");
    sel->setCurrentRow(1);
    waitFor([]{ return false; }, 300);
    const QStringList before = keys();
    const QString current = dm->currentKey;
    const int restoresBefore = libraryRestoreCount;
    fprintf(stderr, "SELFTEST: park: Library rows=%d current=%s\n",
            dm->rowCount(), current.toLocal8Bit().constData());

    /*  2. Folders WITH NOTHING TO GO BACK TO -- the Library opened at start-up, the
        user's first Folders click (MW::showFoldersSource stops before it sets the scope,
        which once cleared the Library before the park decision ran) -- then a folder
        picked, as a person does next. The Library must stay parked through both. */
    lastFolderPath.clear();
    chooseSource(false, "selftest park: Folders");
    if (!waitFor([this]{ return G::scope == G::Scope::Folders && !G::isLoadRunning; },
                 30000))
        fail("Folders", "did not reach the Folders scope");
    if (!dm->hasParkedScope()) fail("Folders", "the Library was not parked");
    if (fsTree->select(folder))
        folderSelectionChange(folder, G::FolderOp::Add, /*resetDataModel*/true, false);
    if (!waitFor(settled(G::Scope::Folders), 30000))
        fail("Folders", QString("folder did not settle: rows=%1").arg(dm->rowCount()));
    if (!dm->hasParkedScope()) fail("Folders", "the folder load dropped the parked Library");
    if (dm->parkedRowCount() != before.size())
        fail("Folders", QString("parked %1 rows of %2").arg(dm->parkedRowCount())
                            .arg(before.size()));

    // 3. back to the Library: restored, whole, on the same image
    chooseSource(true, "selftest park: Library again");
    if (!waitFor(settled(G::Scope::Catalog), 30000))
        fail("restore", QString("did not settle: rows=%1").arg(dm->rowCount()));
    if (libraryRestoreCount != restoresBefore + 1)
        fail("restore", "the Library was reloaded, not restored");
    if (dm->hasParkedScope()) fail("restore", "still parked after the restore");
    if (!dm->verifyIntegrity("selftest park"))
        fail("restore", "verifyIntegrity (see MODELINTEGRITY lines)");
    if (keys() != before)
        fail("restore", QString("rows differ: %1 now, %2 before")
                            .arg(dm->rowCount()).arg(before.size()));
    if (dm->currentKey != current)
        fail("restore", "current image " + dm->currentKey + ", was " + current);
    if (dm->rowSync()->size() != dm->rowCount())
        fail("restore", "worker row view not rebuilt");

    fprintf(stderr, "SELFTEST: PARK PASS\n");
    fflush(stderr);
    std::_Exit(0);
}

void MW::selfTestLibrarySnapshot(const QString &folder)
{
/*
    THE LIBRARY FROM ITS SNAPSHOT FILE (ctest library_snapshot): what the next session's
    first Library does, in one process. The snapshot is written as quit writes it, the
    parked copy is dropped so the return must read the file, and one row is rewritten in
    the index in between -- so the restore must report exactly that row as changed, and
    must still hand back the same rows on the same image.
*/
    auto settled = [this](G::Scope s) {
        return [this, s]{
            return G::scope == s && dm->rowCount() > 0 && !G::isLoadRunning
                   && !G::isModifyingDatamodel && G::allMetadataAttempted
                   && !loadCurtainUp;
        };
    };
    auto keys = [this]{
        QStringList k;
        for (int r = 0; r < dm->rowCount(); ++r)
            k << dm->index(r, 0).data(G::KeyRole).toString();
        k.sort();
        return k;
    };
    auto failS = [](const QString &step, const QString &why) {
        fprintf(stderr, "SELFTEST: SNAPSHOT FAIL [%s] %s\n",
                step.toLocal8Bit().constData(), why.toLocal8Bit().constData());
        fflush(stderr);
        std::_Exit(9);
    };

    if (!selfTestCatalogueFolder(folder)) failS("catalogue", "the fixture was not indexed");
    QFile::remove(librarySnapshotPath());

    chooseSource(true, "selftest snapshot: Library");
    if (!waitFor(settled(G::Scope::Catalog), 30000)) failS("first Library", "did not settle");
    sel->setCurrentRow(1);
    waitFor([]{ return false; }, 300);
    const QStringList before = keys();
    const QString current = dm->currentKey;

    saveLibrarySnapshot();
    if (!QFileInfo::exists(librarySnapshotPath())) failS("save", "no snapshot written");

    chooseSource(false, "selftest snapshot: Folders");
    if (!waitFor(settled(G::Scope::Folders), 30000)) failS("Folders", "did not settle");
    dropParkedLibrary("selftest: the return must read the file");

    // one row rewritten in the index after the snapshot
    const QHash<QString, CatalogRow> one = Catalog::instance().rowsForPaths({before.first()});
    if (one.isEmpty()) failS("index edit", "row not in the index: " + before.first());
    CatalogRow r = one.cbegin().value();
    r.title = "changed after the snapshot";
    r.sidecarMtime += 1;                    // so commit() does not skip it as unchanged
    if (Catalog::instance().commit({r}) != 1) failS("index edit", "commit wrote nothing");

    const int restoresBefore = libraryRestoreCount;
    const int repairsBefore = settledRepairs;
    chooseSource(true, "selftest snapshot: Library again");
    if (!waitFor(settled(G::Scope::Catalog), 30000))
        failS("restore", QString("did not settle: rows=%1 scope=%2 loadRunning=%3"
                                 " modifying=%4 metaAttempted=%5 curtain=%6")
                             .arg(dm->rowCount()).arg(int(G::scope))
                             .arg(bool(G::isLoadRunning)).arg(bool(G::isModifyingDatamodel))
                             .arg(bool(G::allMetadataAttempted)).arg(loadCurtainUp));
    if (libraryRestoreCount != restoresBefore + 1)
        failS("restore", "the Library was loaded, not restored from the snapshot");
    if (lastSnapshotChanged != 1)
        failS("restore", QString("%1 rows reported changed, expected 1")
                             .arg(lastSnapshotChanged));
    if (!dm->verifyIntegrity("selftest snapshot"))
        failS("restore", "verifyIntegrity (see MODELINTEGRITY lines)");
    if (keys() != before)
        failS("restore", QString("rows differ: %1 now, %2 before")
                             .arg(dm->rowCount()).arg(before.size()));
    if (dm->currentKey != current)
        failS("restore", "current image " + dm->currentKey + ", was " + current);

    /*  THE CHANGED ROW IS RE-READ, and the load settles again after it. Called straight
        after the restore, this repair asked MetaRead for a re-read that never came, and
        G::allMetadataAttempted stayed false for good (MW::repairRowsWhenSettled). */
    if (!waitFor([this, repairsBefore]{ return settledRepairs > repairsBefore; }, 35000))
        failS("repair", "the changed row was never handed to refreshStaleRows");
    if (!waitFor(settled(G::Scope::Catalog), 30000))
        failS("repair", QString("did not settle after the re-read: metaAttempted=%1")
                            .arg(bool(G::allMetadataAttempted)));
    if (keys() != before) failS("repair", "rows differ after the re-read");

    fprintf(stderr, "SELFTEST: SNAPSHOT PASS\n");
    fflush(stderr);
    std::_Exit(0);
}
