#include "Main/mainwindow.h"
#include "Utilities/versionkey.h"

/*
    WINNOW_SELFTEST_NAV=1 -- the click-through probe (ctest selftest_nav).

    Selects rows one at a time, the way a user clicks thumbnails, and reports for each
    click what the user would see: the current key, the image the loupe is showing,
    whether the ImageCache holds the current image, and the cached / icon counts.

    WHY IT EXISTS. Every other self-test stops once a folder has loaded. A regression
    that left the loupe on the previous image after a click, with the ImageCache no
    longer caching and some thumbnails never arriving, passed all of them -- a person
    browsing found it. A browse that does not follow the selection is the one failure
    that makes Winnow useless, so it is checked directly.
*/

namespace {

void settleNav(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
}

}  // namespace

void MW::selfTestNavigate()
{
    /*  WINNOW_SELFTEST_NAV_REFRESH=1 runs the menu Refresh (MW::applyModelChange with
        reconcileDisk) first: any on-the-fly model change re-instances the model, and the
        clicks after it must still be followed. */
    if (qEnvironmentVariableIntValue("WINNOW_SELFTEST_NAV_REFRESH") == 1) {
        refresh();
        settleNav(1500);
        fprintf(stderr, "SELFTEST: nav after Refresh, instance=%d\n", int(dm->instance));
    }
    const int n = dm->sf->rowCount();
    const int clicks = qMin(n, 8);
    int failures = 0;
    for (int r = 0; r < clicks; ++r) {
        const QModelIndex idx = dm->sf->index(r, 0);
        const QString key = idx.data(G::KeyRole).toString();
        sel->select(idx, Qt::NoModifier, "selftest nav");
        /*  Long enough for a raw: a sensor decode is seconds, and the loupe shows the
            image only once the ImageCache has it. WINNOW_SELFTEST_CLICK_MS overrides. */
        settleNav(qEnvironmentVariableIsSet("WINNOW_SELFTEST_CLICK_MS")
                      ? qEnvironmentVariableIntValue("WINNOW_SELFTEST_CLICK_MS") : 6000);

        int cached = 0, icons = 0;
        for (int i = 0; i < dm->rowCount(); ++i) {
            if (dm->index(i, G::IsCachedColumn).data().toBool()) ++cached;
            if (!dm->index(i, 0).data(Qt::DecorationRole).isNull()) ++icons;
        }
        const bool video = idx.sibling(r, G::VideoColumn).data().toBool();
        const bool current = dm->currentKey == key;
        const bool shown = video || imageView->currentImagePath == key;
        const bool inCache = video || icd->contains(key);
        fprintf(stderr, "SELFTEST: nav row=%d current=%d shown=%d inCache=%d cached=%d "
                        "icons=%d/%d instance=%d suspended=%d allMeta=%d removing=%d %s\n",
                r, int(current), int(shown), int(inCache), cached, icons,
                dm->rowCount(), int(dm->instance), int(dm->sf->isSuspended()),
                int(G::allMetadataAttempted), int(G::removingRowsFromDM),
                QFileInfo(VersionKey::sourceOf(key)).fileName().toLocal8Bit().constData());
        if (!current || !shown || !inCache) {
            /*  The first failure dumps the ImageCache's own view -- its health checks
                (decoder instances among them) and parameters -- because "not cached"
                has several causes and the fix depends on which. */
            if (failures++ == 0) {
                fprintf(stderr, "SELFTEST: nav diagnostics\n%s\n%s\n",
                        imageCache->reportHealthChecks().toLocal8Bit().constData(),
                        imageCache->reportCacheParameters().toLocal8Bit().constData());
                fprintf(stderr, "%s\n%s\n",
                        imageCache->reportCacheDecoders().toLocal8Bit().constData(),
                        imageCache->reportCacheItemList().toLocal8Bit().constData());
            }
        }
    }
    fprintf(stderr, failures ? "SELFTEST: NAV FAIL %d of %d clicks\n"
                             : "SELFTEST: NAV PASS %d of %d clicks\n",
            failures ? failures : clicks, clicks);
    fflush(stderr);
}
