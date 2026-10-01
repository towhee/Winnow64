#include "Main/mainwindow.h"
#include "Metadata/versions.h"
#include "Utilities/versionkey.h"
#include "Utilities/fileops.h"
#include "Develop/editstack.h"

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
            if (dm->rowFromKey(path(n)) < 0) fail(step, n + " is not in the model");
        for (const QString &n : absent)
            if (dm->rowFromKey(path(n)) >= 0) fail(step, n + " is still in the model");

        for (const QString &n : loaded) {
            const int r = dm->rowFromKey(path(n));
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
                dm->index(r, G::PathColumn).data(G::KeyRole).toString(), combineRawJpg);
            if (r && k < prev) fail(step, QString("source row %1 is out of order").arg(r));
            prev = k;
        }

        /*  The proxy mirrors source order: no filter is active and the default sort has
            no proxy sort column. A row "landing at the end" shows up right here. The one
            source row the proxy may leave out is a version whose group is collapsed. */
        if (!filters->isAnyFilter()) {
            int shown = 0;
            for (int r = 0; r < n; ++r) {
                const QString k = dm->index(r, G::PathColumn).data(G::KeyRole).toString();
                if (!VersionKey::isVersion(k)
                    || dm->sf->versionsExpanded(VersionKey::sourceOf(k))) ++shown;
            }
            if (dm->sf->rowCount() != shown)
                fail(step, QString("proxy has %1 rows, expected %2 of %3")
                               .arg(dm->sf->rowCount()).arg(shown).arg(n));
            if (dm->sf->sortColumn() == G::NameColumn || dm->sf->sortColumn() < 0) {
                int prevSrc = -1;
                for (int r = 0; r < dm->sf->rowCount(); ++r) {
                    const int src = dm->sf->mapToSource(dm->sf->index(r, 0)).row();
                    if (src <= prevSrc) {
                        for (int q = 0; q < dm->sf->rowCount(); ++q)
                            fprintf(stderr, "SELFTEST: mutation proxy %d -> source %d %s\n", q,
                                dm->sf->mapToSource(dm->sf->index(q, 0)).row(),
                                dm->sf->index(q, 0).data(G::KeyRole).toString().toLocal8Bit().constData());
                        fail(step, QString("proxy row %1 is out of source order").arg(r));
                    }
                    prevSrc = src;
                }
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

    /*  VERSIONS (virtual copies) -- rows keyed path + "/#v" + id, filled from their
        master, collapsed behind it (Main/mwversions.cpp). The versions are written to
        the master's sidecar here and picked up by re-reading the master, which is how
        they arrive when another window or machine created them. */
    const QString master = path("sample01.jpg");
    const QString v1 = VersionKey::make(master, 1), v2 = VersionKey::make(master, 2);
    const QString v1n = "sample01.jpg/#v1", v2n = "sample01.jpg/#v2";
    const QString masterRating = dm->index(dm->rowFromKey(master), G::RatingColumn)
                                     .data().toString();

    // V1  a master's versions appear as rows after it, collapsed, filled from it
    if (!Versions::update(master, [](VersionSet &set) {
            ImageVersion a;
            a.name = "B&W";
            a.rating = 4;
            a.label = "Red";
            a.pick = "Picked";
            set.add(a);
            set.add(ImageVersion());
        }))
        fail("versions appear", "could not write the versions");
    insertFiles({master});                          // re-read the master's sidecar
    settle(400);                                    // the coalesced reconcile
    check("versions appear", {"sample01.jpg", v1n, v2n}, {}, {"sample01.jpg", v1n, v2n});
    {
        const int mr = dm->rowFromKey(master), r1 = dm->rowFromKey(v1),
                  r2 = dm->rowFromKey(v2);
        if (!(mr < r1 && r1 < r2 && r2 == mr + 2))
            fail("versions appear", QString("rows %1 %2 %3 are not a group")
                                        .arg(mr).arg(r1).arg(r2));
        if (dm->index(mr, 0).data(G::VersionCountRole).toInt() != 2)
            fail("versions appear", "master's version count is not 2");
        if (dm->index(r1, 0).data(G::VersionNameRole).toString() != "B&W")
            fail("versions appear", "v1 has not got its name");
        if (dm->index(r1, G::RatingColumn).data().toString() != "4"
            || dm->index(r1, G::LabelColumn).data().toString() != "Red"
            || dm->index(r1, G::PickColumn).data().toString() != "Picked")
            fail("versions appear", "v1 has not got its own rating, label and pick");
        if (dm->index(r2, G::RatingColumn).data().toString() != "")
            fail("versions appear", "v2 should be unrated");
        if (dm->index(mr, G::RatingColumn).data().toString() != masterRating)
            fail("versions appear", "the master's rating changed");
        if (dm->index(r1, G::WidthColumn).data() != dm->index(mr, G::WidthColumn).data())
            fail("versions appear", "v1 did not take the master's metadata");
        if (dm->index(r1, 0).data(G::SourcePathRole).toString() != master)
            fail("versions appear", "v1's source is not the master's file");
        if (dm->proxyRowFromKey(v1) >= 0)
            fail("versions appear", "v1 is visible in a collapsed group");
    }

    /*  V1b The version rows went in MID-MODEL, after the load. Clicking an image past
            them must still reach the ImageCache and the loupe: a mid-model insert once
            left the worker-side row flags shifted by one and discarded decodes counted as
            attempts, and the loupe stopped following the selection. */
    {
        const QString after = path("sample02.tif");
        sel->select(dm->proxyIndexFromKey(after), Qt::NoModifier, "selftest");
        settle(3000);
        if (dm->currentKey != after) fail("click after versions", "selection did not move");
        if (!icd->contains(after))
            fail("click after versions", "the ImageCache did not decode the clicked image\n"
                 + imageCache->reportHealthChecks());
        if (imageView->currentImagePath != after)
            fail("click after versions", "the loupe is showing " + imageView->currentImagePath);
        fprintf(stderr, "SELFTEST: mutation [click after versions] ok\n");
    }

    // V2  expanding shows the group; the versions follow the master in the proxy
    setVersionsExpanded(master, true);
    check("versions expand", {v1n, v2n}, {}, {});
    if (dm->proxyRowFromKey(v1) != dm->proxyRowFromKey(master) + 1)
        fail("versions expand", "v1 is not right after its master in the proxy");

    // V3  collapsing while a version is current lands on its master
    sel->select(dm->proxyIndexFromKey(v2), Qt::NoModifier, "selftest");
    settle(200);
    setVersionsExpanded(master, false);
    check("versions collapse", {v1n, v2n}, {}, {});
    if (dm->currentKey != master)
        fail("versions collapse", "current is " + dm->currentKey + ", not the master");

    // V4  a version deleted in the sidecar leaves the model; its id is not reused
    if (!Versions::update(master, [](VersionSet &set) { set.remove(2); }))
        fail("version removed", "could not rewrite the versions");
    insertFiles({master});
    settle(400);
    check("version removed", {"sample01.jpg", v1n}, {v2n}, {});
    if (dm->index(dm->rowFromKey(master), 0).data(G::VersionCountRole).toInt() != 1)
        fail("version removed", "master's version count is not 1");

    // V5  the menu Refresh leaves version rows alone (they are never on disk)
    refresh();
    check("versions survive refresh", {"sample01.jpg", v1n}, {}, {});

    // V6  removing the master takes its versions with it
    QFile::remove(master);
    QFile::remove(master + ".xmp");
    refreshAfterRemoval({master});
    check("master removed with versions", {}, {"sample01.jpg", v1n}, {});

    /*  V7  New Version (Develop > Versions, Ctrl+') on the current image: a copy of its
            recipe and values, shown and selected so the next edit goes to it. */
    const QString m2 = path("sample015.jpg");
    const QString m2v1 = VersionKey::make(m2, 1);
    sel->select(dm->proxyIndexFromKey(m2), Qt::NoModifier, "selftest");
    settle(200);
    newVersion();
    check("new version", {"sample015.jpg", "sample015.jpg/#v1"}, {},
          {"sample015.jpg", "sample015.jpg/#v1"});
    if (dm->currentKey != m2v1)
        fail("new version", "current is " + dm->currentKey + ", not the new version");
    if (dm->proxyRowFromKey(m2v1) < 0)
        fail("new version", "the new version is not shown");
    if (Metadata::readDevelopSidecar(m2v1) != Metadata::readDevelopSidecar(m2))
        fail("new version", "the new version did not start from the master's recipe");

    /*  V8  Set Version as Master swaps the two recipes: the version's edit becomes the
            one in winnow:Develop, the master's old one lives on in the version. */
    EditStack edited;
    edited.scopes.append(EditScope());
    edited.scopes[0].params.exposure = 1.0f;
    const QString blob = edited.toBase64();
    Metadata::writeDevelopSidecar(m2v1, blob, QString());
    if (Metadata::readDevelopSidecar(m2v1) != blob)
        fail("set as master", "could not give the version a recipe");
    setVersionAsMaster();
    check("set as master", {"sample015.jpg", "sample015.jpg/#v1"}, {}, {});
    if (Metadata::readDevelopSidecar(m2) != blob)
        fail("set as master", "the master did not take the version's recipe");
    if (!Metadata::readDevelopSidecar(m2v1).isEmpty())
        fail("set as master", "the version did not take the master's (empty) recipe");
    if (!dm->index(dm->rowFromKey(m2), G::DevelopColumn).data().toBool()
        || dm->index(dm->rowFromKey(m2v1), G::DevelopColumn).data().toBool())
        fail("set as master", "the develop badges did not swap");
    settle(200);
    if (dm->currentKey != m2)
        fail("set as master", "the selection did not follow the edit to the master");

    /*  V9  A version's rating, label and pick are its OWN: set through the model (the
            path every writer takes -- keys, menus, undo, recovery) they land in its
            record, and the master's rating and file are untouched. */
    {
        const int vr = dm->rowFromKey(m2v1), mr = dm->rowFromKey(m2);
        const QString masterRatingBefore = dm->index(mr, G::RatingColumn).data().toString();
        const QString sidecarBefore = [&] {
            QFile f(FileOps::sidecarPath(m2));
            return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
        }();
        const QRegularExpression rating(R"re(xmp:Rating\s*=\s*"(\d)")re");
        const QString masterFileRating = rating.match(sidecarBefore).captured(1);
        emit setValDm(vr, G::RatingColumn, "3", dm->instance, "selftest", Qt::EditRole);
        emit setValDm(vr, G::LabelColumn, "Green", dm->instance, "selftest", Qt::EditRole);
        emit setValDm(vr, G::PickColumn, "Rejected", dm->instance, "selftest", Qt::EditRole);
        check("version values", {"sample015.jpg", "sample015.jpg/#v1"}, {}, {});
        const ImageVersion rec = Versions::readVersion(m2, 1);
        if (rec.rating != 3 || rec.label != "Green" || rec.pick != "Rejected")
            fail("version values", QString("record holds %1 / %2 / %3")
                                       .arg(rec.rating).arg(rec.label, rec.pick));
        if (dm->index(mr, G::RatingColumn).data().toString() != masterRatingBefore)
            fail("version values", "the master's rating changed");
        QFile f(FileOps::sidecarPath(m2));
        const QString sidecarAfter =
            f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
        if (rating.match(sidecarAfter).captured(1) != masterFileRating)
            fail("version values", "xmp:Rating (the master's) changed");
    }

    /*  V10 Keywords are the FILE's: tagging with only the version selected writes the
            master's file and shows on both rows. */
    sel->select(dm->proxyIndexFromKey(m2v1), Qt::NoModifier, "selftest");
    settle(200);
    applyKeywordsToSelection({"selftestkw"}, {});
    check("shared keywords", {"sample015.jpg", "sample015.jpg/#v1"}, {}, {});
    for (const QString &k : {m2, m2v1})
        if (!dm->index(dm->rowFromKey(k), G::KeywordsColumn).data().toStringList()
                 .contains("selftestkw"))
            fail("shared keywords", k + " does not show the keyword");

    /*  V11 Rotating the version turns the file and every row of its group. */
    const int orientBefore =
        dm->index(dm->rowFromKey(m2), G::OrientationColumn).data().toInt();
    setRotation(90);
    check("rotate group", {"sample015.jpg", "sample015.jpg/#v1"}, {}, {});
    const int orientM = dm->index(dm->rowFromKey(m2), G::OrientationColumn).data().toInt();
    const int orientV = dm->index(dm->rowFromKey(m2v1), G::OrientationColumn).data().toInt();
    if (orientM == orientBefore || orientM != orientV)
        fail("rotate group", QString("orientation %1 -> master %2, version %3")
                                 .arg(orientBefore).arg(orientM).arg(orientV));
    setRotation(270);                               // and back

    /*  V12 Re-reading the master refills its versions from the sidecar: nothing set
            above may revert. */
    settle(300);                                    // the orientation write is pooled
    insertFiles({m2});
    settle(400);
    check("refill keeps edits", {"sample015.jpg", "sample015.jpg/#v1"}, {}, {});
    {
        const int vr = dm->rowFromKey(m2v1);
        if (dm->index(vr, G::RatingColumn).data().toString() != "3"
            || dm->index(vr, G::LabelColumn).data().toString() != "Green"
            || dm->index(vr, G::PickColumn).data().toString() != "Rejected")
            fail("refill keeps edits", "the version's own values reverted");
        if (!dm->index(vr, G::KeywordsColumn).data().toStringList().contains("selftestkw"))
            fail("refill keeps edits", "the version lost the file's keyword");
    }

    /*  V13 Renaming a master with versions (the Rename dialog's in-place update,
            RenameFileDlg::renameDatamodel): the file and its sidecar move, and the
            version rows are re-keyed onto the new path. The name keeps the row in sort
            position, as the dialog leaves rows where they are. */
    const QString m3 = path("sample016.jpg");
    const QString m3v1 = VersionKey::make(m3, 1);
    if (!FileOps::moveFile(m2, m3)) fail("rename master", "could not move the file");
    /*  The thumbnails must follow the re-key: the icon store is keyed by path, and a
        re-key that left them under the old one showed a blank cell for good. */
    const bool hadIcon = !dm->index(dm->rowFromKey(m2), 0)
                              .data(Qt::DecorationRole).isNull();
    const bool hadVersionIcon = !dm->index(dm->rowFromKey(m2v1), 0)
                                     .data(Qt::DecorationRole).isNull();
    {
        const int row = dm->rowFromKey(m2);
        dm->fPathRow.remove(m2);
        dm->fPathRow[m3] = row;
        dm->setData(dm->index(row, G::PathColumn), m3, G::KeyRole);
        dm->setData(dm->index(row, G::NameColumn), QFileInfo(m3).fileName());
        for (const auto &kv : dm->rekeyVersions(m2, m3)) imageCache->rename(kv.first, kv.second);
    }
    check("rename master", {"sample016.jpg", "sample016.jpg/#v1"},
          {"sample015.jpg", "sample015.jpg/#v1"}, {});
    if (Versions::read(m3).find(1) == nullptr)
        fail("rename master", "the version did not travel with the sidecar");
    if (dm->index(dm->rowFromKey(m3v1), G::RatingColumn).data().toString() != "3")
        fail("rename master", "the version row lost its own values");
    if (hadIcon && dm->index(dm->rowFromKey(m3), 0).data(Qt::DecorationRole).isNull())
        fail("rename master", "the thumbnail stayed under the old path");
    if (hadVersionIcon
        && dm->index(dm->rowFromKey(m3v1), 0).data(Qt::DecorationRole).isNull())
        fail("rename master", "the version's thumbnail stayed under the old key");
    if (!hadIcon)
        qDebug().noquote() << "SELFTEST: rename master: no thumbnail loaded, icon check skipped";

    /*  V14 Deleting a selected VERSION removes its record, never the image file. */
    deleteVersionRecords({m3v1}, /*confirm*/false);
    check("delete version", {"sample016.jpg"}, {"sample016.jpg/#v1"}, {});
    if (!QFile::exists(m3)) fail("delete version", "the image file went");
    if (Versions::read(m3).find(1) != nullptr)
        fail("delete version", "the version's record is still there");

    // 6  everything removed
    QStringList all;
    for (const QString &n : dir.entryList(QDir::Files)) {
        all << path(n);
        QFile::remove(path(n));
    }
    refreshAfterRemoval(all);
    check("remove all", {}, {"sample016.jpg", "sample016.jpg/#v1"}, {});
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
