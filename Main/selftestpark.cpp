#include "Main/mainwindow.h"
#include "Cache/catalog.h"
#include "Metadata/indexmetadata.h"

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
    int files = 0;      // dot files are never catalogued (catalogCandidateName)
    for (const QString &name : QDir(folder).entryList(QDir::Files))
        if (!name.startsWith(QLatin1Char('.'))) ++files;
    /*  THIS FOLDER'S ROWS, not the whole index: an index holding another folder's rows
        (an earlier test's fixture) passed a whole-index count at once, so the scan never
        ran and the Library held none of this folder. Both spellings of the path, as the
        scanner may record either (macOS /var is a link to /private/var). */
    const QString canon = QFileInfo(folder).canonicalFilePath();
    auto under = [folder, canon]{
        int n = Catalog::instance().countUnder(folder, false);
        if (!canon.isEmpty() && canon != folder)
            n += Catalog::instance().countUnder(canon, false);
        return n;
    };
    if (under() < files) startCatalogScan();
    /*  AND THE SCAN HAS FINISHED. It runs on its own thread, so a caller draining the
        pool does not wait for it, and a scan still committing rows it stat'd earlier
        overwrites anything written since: rotate_stamp's restamp, intermittently. */
    return waitFor([this, files, under]{
        return under() >= files && !(catalogScanner && catalogScanner->isRunning());
    }, 30000);
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
    /*  NOT SETTLED UNTIL THE RESTORE HAS HAPPENED. The snapshot is read on a pool thread
        (startLibrarySnapshotRestore), and until it lands the Folders model is still up
        and idle with the scope already Catalog, so settled() alone passed at once and
        the restore count was read before the restore: an intermittent false failure.
        A Library that was LOADED instead never counts a restore, so it times out here
        and is reported as that. */
    const auto restored = settled(G::Scope::Catalog);
    if (!waitFor([&]{ return libraryRestoreCount != restoresBefore && restored(); },
                 30000)) {
        if (libraryRestoreCount == restoresBefore)
            failS("restore", "the Library was loaded, not restored from the snapshot");
        failS("restore", QString("did not settle: rows=%1 scope=%2 loadRunning=%3"
                                 " modifying=%4 metaAttempted=%5 curtain=%6")
                             .arg(dm->rowCount()).arg(int(G::scope))
                             .arg(bool(G::isLoadRunning)).arg(bool(G::isModifyingDatamodel))
                             .arg(bool(G::allMetadataAttempted)).arg(loadCurtainUp));
    }
    if (libraryRestoreCount != restoresBefore + 1)
        failS("restore", "the Library was restored more than once");
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

void MW::selfTestRotateStamp(const QString &folder)
{
/*
    A ROTATION IN THE LIBRARY IS NOT AN OUTSIDE EDIT (ctest rotate_stamp, finding C18,
    Documentation 4.4.17). Rotating writes the orientation on a pool thread -- to the XMP
    sidecar, or with "Permit image file modification" into the JPEG itself -- and that
    moves a stamp the catalog holds. Before the restamp, ScrollVerify's next pass found
    the row "out of date with its file" and refreshStaleRows threw away its metadata, icon
    and decode: the image just rotated, usually the current one.

    Both write paths, on a copy of the fixture (runSelfTest makes it). After each:
    - the catalog's stamps equal the file's, compared as ScrollVerify compares them
      (IndexMetadata::candidate);
    - past the own-write grace (kOwnWriteGraceMs), which would otherwise hide a missed
      restamp as "ours", a whole-set verify reports nothing stale.
*/
    auto failR = [&folder](const QString &step, const QString &why) {
        fprintf(stderr, "SELFTEST: ROTATE FAIL [%s] %s\n",
                step.toLocal8Bit().constData(), why.toLocal8Bit().constData());
        fflush(stderr);
        QDir(folder).removeRecursively();
        std::_Exit(10);
    };
    auto settled = [this]{
        return G::scope == G::Scope::Catalog && dm->rowCount() > 0 && !G::isLoadRunning
               && !G::isModifyingDatamodel && G::allMetadataAttempted && !loadCurtainUp;
    };
    /*  The write runs on the pool, reports back queued, and the restamp commits on the
        pool again: drain, let the queued call run, drain. */
    auto drain = []{
        QThreadPool::globalInstance()->waitForDone(15000);
        waitFor([]{ return false; }, 300);
        QThreadPool::globalInstance()->waitForDone(15000);
    };

    if (!selfTestCatalogueFolder(folder)) failR("catalogue", "the fixture was not indexed");
    /*  A LOADED Library, not one restored: the test-mode snapshot file outlives the run
        that wrote it (library_snapshot leaves one holding the fixture's rows), and a
        restore would open those instead of this copy. */
    QFile::remove(librarySnapshotPath());
    dropParkedLibrary("selftest rotate: load the Library fresh");
    chooseSource(true, "selftest rotate: Library");
    if (!waitFor(settled, 30000)) failR("Library", "did not settle");
    if (!scrollVerify) failR("Library", "no ScrollVerify");
    G::useScrollInVerify = true;

    // either spelling: macOS /var is a link to /private/var
    QString path = QDir(QFileInfo(folder).canonicalFilePath()).filePath("sample01.jpg");
    if (dm->rowFromKey(path) < 0) path = QDir(folder).filePath("sample01.jpg");
    const int dmRow = dm->rowFromKey(path);
    if (dmRow < 0) {
        QStringList k;
        for (int r = 0; r < dm->rowCount() && r < 8; ++r)
            k << dm->index(r, 0).data(G::KeyRole).toString();
        failR("Library", path + " is not in the Library; it holds " +
                         QString::number(dm->rowCount()) + ": " + k.join(", "));
    }
    sel->setCurrentRow(dm->proxyRowFromModelRow(dmRow));
    waitFor([]{ return false; }, 300);

    QStringList staleSeen;
    const QMetaObject::Connection watch =
        connect(scrollVerify, &ScrollVerify::rowsAreStale, this,
                [&staleSeen](const QStringList &paths){ staleSeen << paths; });

    const bool modifyWas = G::modifySourceFiles;
    for (const bool modify : {false, true}) {
        const QString step = modify ? "rotate, file written" : "rotate, sidecar written";
        G::modifySourceFiles = modify;
        const qint64 fileMtime = QFileInfo(path).lastModified().toMSecsSinceEpoch();

        setRotation(90);
        drain();
        if (modify && QFileInfo(path).lastModified().toMSecsSinceEpoch() == fileMtime)
            fprintf(stderr, "SELFTEST: rotate note: [%s] the image file did not change\n",
                    step.toLocal8Bit().constData());

        auto stampsMatch = [this, &path](QString *why) {
            const CatalogRow cand = IndexMetadata::candidate(QFileInfo(path), metadata);
            const QHash<QString, CatalogRow> held =
                Catalog::instance().rowsForPaths({path});
            if (held.isEmpty()) { if (why) *why = "the row left the catalog"; return false; }
            const CatalogRow &h = held.cbegin().value();
            const bool ok = h.srcSize == cand.srcSize && h.srcMtime == cand.srcMtime
                            && h.sidecarMtime == cand.sidecarMtime;
            if (!ok && why)
                *why = QString("size %1/%2 mtime %3/%4 sidecar %5/%6 (catalog/file)")
                           .arg(h.srcSize).arg(cand.srcSize)
                           .arg(h.srcMtime).arg(cand.srcMtime)
                           .arg(h.sidecarMtime).arg(cand.sidecarMtime);
            return ok;
        };
        QString why;
        if (!stampsMatch(&why)) {
            /*  Still a failure, but say which kind: a restamp that lands late (test
                timing) reads differently from one that never comes (the bug). */
            QElapsedTimer late;
            late.start();
            const bool landed = waitFor([&]{ return stampsMatch(nullptr); }, 3000);
            failR(step, "catalog stamps not restamped after the write: " + why
                        + (landed ? QString("; they matched %1 ms later").arg(late.elapsed())
                                  : QString("; still not after 3 s"))
                        + QString("; in scope %1")
                              .arg(isCatalogScopeFolder(QFileInfo(path).absolutePath())));
        }

        waitFor([]{ return false; }, int(kOwnWriteGraceMs) + 1000);
        staleSeen.clear();
        /*  reset() first: the load's own sweep has just verified every row, and a row
            verified in the last kReverifySecs is skipped. The sweep starts after
            kWholeSetStartMs (3 s). */
        scrollVerify->reset();
        scrollVerify->verifyWholeSet();
        waitFor([]{ return false; }, 6000);
        QThreadPool::globalInstance()->waitForDone(15000);
        waitFor([]{ return false; }, 300);
        if (staleSeen.contains(path))
            failR(step, "ScrollVerify reported the rotated image stale");
        /*  Only this image is the test's. The test catalog persists between runs, so
            the Library also holds rows of earlier runs' deleted copies. */
        if (!staleSeen.isEmpty())
            fprintf(stderr, "SELFTEST: rotate note: [%s] %lld other row(s) stale\n",
                    step.toLocal8Bit().constData(), qlonglong(staleSeen.size()));
        fprintf(stderr, "SELFTEST: rotate [%s] clean\n", step.toLocal8Bit().constData());
    }
    G::modifySourceFiles = modifyWas;
    disconnect(watch);

    fprintf(stderr, "SELFTEST: ROTATE PASS\n");
    fflush(stderr);
    QDir(folder).removeRecursively();
    std::_Exit(0);
}
