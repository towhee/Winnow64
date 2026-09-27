#include "Main/mainwindow.h"

#include <cstdlib>          // std::_Exit

/*
    WINNOW_SELFTEST_MUTATION=1 -- the on-the-fly insert/delete self-test (ctest
    model_mutation). See MW::applyModelChange and "DataModel On-the-Fly Insert and Delete"
    in notes/Documentation.txt.

    WHY IT EXISTS. Inserting into and deleting from a loaded datamodel broke four times
    in four different ways -- a new row landing at the end of the proxy (twice), the
    second of two focus stacks deleted by refresh(), the filters built before the rows
    had metadata -- and every one was found by a person looking at a thumbnail grid.
    Each step here is one of the shapes those callers produce, and each is checked for
    the structural state they left wrong.

    RUNS ON A COPY. runSelfTest copies the fixture folder to a temporary folder and loads
    that, because the steps create and delete files.
*/

namespace {

void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
}

[[noreturn]] void fail(const QString &step, const QString &why)
{
    fprintf(stderr, "SELFTEST: MUTATION FAIL [%s] %s\n",
            step.toLocal8Bit().constData(), why.toLocal8Bit().constData());
    fflush(stderr);
    std::_Exit(7);
}

bool copyOver(const QString &from, const QString &to)
{
    QFile::remove(to);
    return QFile::copy(from, to);
}

}  // namespace

void MW::selfTestModelMutation(const QString &folder)
{
    const QDir dir(folder);
    auto path = [&](const QString &name) { return dir.filePath(name); };

    /*  The checks every step ends with. present/absent are file names in the test folder;
        loaded are the ones that must have come out of the pipeline MetaLoaded. */
    auto check = [&](const QString &step, const QStringList &present,
                     const QStringList &absent, const QStringList &loaded) {
        settle(400);
        if (!dm->verifyIntegrity("selftest " + step))
            fail(step, "verifyIntegrity (see MODELINTEGRITY lines)");

        for (const QString &n : present)
            if (dm->rowFromPath(path(n)) < 0) fail(step, n + " is not in the model");
        for (const QString &n : absent)
            if (dm->rowFromPath(path(n)) >= 0) fail(step, n + " is still in the model");

        for (const QString &n : loaded) {
            const int r = dm->rowFromPath(path(n));
            if (dm->index(r, G::MetadataStatusColumn).data().toInt() != G::MetaLoaded)
                fail(step, n + " metadata not loaded");
            if (dm->index(r, 0).data(Qt::DecorationRole).isNull())
                fprintf(stderr, "SELFTEST: mutation [%s] note: %s has no icon\n",
                        step.toLocal8Bit().constData(), n.toLocal8Bit().constData());
        }

        // source order is the model's sort order (one folder, so globally sorted)
        const int n = dm->rowCount();
        QString prev;
        for (int r = 0; r < n; ++r) {
            const QString k = DataModel::sortKey(
                dm->index(r, G::PathColumn).data(G::PathRole).toString(), combineRawJpg);
            if (r && k < prev) fail(step, QString("source row %1 is out of order").arg(r));
            prev = k;
        }

        /*  The proxy mirrors source order: no filter is active and the default sort has
            no proxy sort column. A row "landing at the end" shows up right here. */
        if (!filters->isAnyFilter()) {
            if (dm->sf->rowCount() != n)
                fail(step, QString("proxy has %1 rows, model %2")
                               .arg(dm->sf->rowCount()).arg(n));
            if (dm->sf->sortColumn() == G::NameColumn || dm->sf->sortColumn() < 0) {
                for (int r = 0; r < n; ++r)
                    if (dm->sf->mapToSource(dm->sf->index(r, 0)).row() != r)
                        fail(step, QString("proxy row %1 is not source row %1").arg(r));
            }
        }
        fprintf(stderr, "SELFTEST: mutation [%s] ok rows=%d\n",
                step.toLocal8Bit().constData(), n);
        fflush(stderr);
    };

    const QStringList start = dir.entryList(QDir::Files, QDir::Name);
    if (dm->rowCount() != start.size())
        fail("start", QString("loaded %1 rows for %2 files")
                          .arg(dm->rowCount()).arg(start.size()));
    if (!start.contains("sample01.jpg") || !start.contains("sample02.tif"))
        fail("start", "fixture files missing");

    // 1  one new file that sorts FIRST
    copyOver(path("sample01.jpg"), path("aaa_first.jpg"));
    insertFiles({path("aaa_first.jpg")});
    check("insert first", {"aaa_first.jpg"}, {}, {"aaa_first.jpg"});

    /*  2  TWO at once, one between existing rows and one at the end -- the shape that
           lost the second focus stack. */
    copyOver(path("sample01.jpg"), path("sample015.jpg"));
    copyOver(path("sample03.png"), path("zzz_last.png"));
    insertFiles({path("zzz_last.png"), path("sample015.jpg")});
    check("insert two", {"sample015.jpg", "zzz_last.png"}, {},
          {"sample015.jpg", "zzz_last.png"});

    // 3  a file rewritten in place (a re-run focus stack, a re-embellish)
    settle(1100);                               // a newer mtime than the load's
    copyOver(path("sample01.jpg"), path("aaa_first.jpg"));
    insertFiles({path("aaa_first.jpg")});
    check("replace in place", {"aaa_first.jpg"}, {}, {"aaa_first.jpg"});

    // 4  scattered removals -- the first row and the last
    QFile::remove(path("aaa_first.jpg"));
    QFile::remove(path("zzz_last.png"));
    refreshAfterRemoval({path("aaa_first.jpg"), path("zzz_last.png")});
    check("remove scattered", {"sample015.jpg"}, {"aaa_first.jpg", "zzz_last.png"}, {});

    // 5  changes made OUTSIDE Winnow, found by the menu Refresh
    copyOver(path("sample01.jpg"), path("ext_added.jpg"));
    refresh();
    check("refresh finds added", {"ext_added.jpg"}, {}, {"ext_added.jpg"});
    QFile::remove(path("ext_added.jpg"));
    refresh();
    check("refresh finds removed", {}, {"ext_added.jpg"}, {});

    // 6  everything removed
    QStringList all;
    for (const QString &n : dir.entryList(QDir::Files)) {
        all << path(n);
        QFile::remove(path(n));
    }
    refreshAfterRemoval(all);
    check("remove all", {}, {"sample01.jpg", "sample015.jpg"}, {});
    if (dm->rowCount() != 0)
        fail("remove all", QString("%1 rows left").arg(dm->rowCount()));

    // 7  insert into the now-empty model
    const QString fixtures = qEnvironmentVariable("WINNOW_SELFTEST_MUTATION_SRC");
    if (!copyOver(QDir(fixtures).filePath("sample01.jpg"), path("back.jpg")))
        fail("insert into empty", "could not copy back.jpg from " + fixtures);
    insertFiles({path("back.jpg")});
    check("insert into empty", {"back.jpg"}, {}, {"back.jpg"});

    fprintf(stderr, "SELFTEST: MUTATION PASS\n");
    fflush(stderr);
}
