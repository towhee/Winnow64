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

    runSelfTest has loaded the fixture folder first, which is what puts it in the (test
    mode) catalog; this makes it the Library's one root and drives the Module dock's
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

    CatalogScopeEntry e;
    e.path = catalogScopeNormalize(folder);
    catalogScope = CatalogScope{e};

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
