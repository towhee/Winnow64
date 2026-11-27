#include "Main/mainwindow.h"
#include "Image/slidepreview.h"
#include "Cache/devpreviewcache.h"
#include <QtConcurrent>
#include <QFutureWatcher>

void MW::slideShow()
{
    if (G::isLogger) G::log("MW::slideShow");
    if (G::isSlideShow) {
        // stop slideshow
        G::popup->showPopup("Slideshow has been terminated.", 2000);
        G::isSlideShow = false;
        slideCount = 0;
        G::useImageCache = prevUseImageCache;
        QApplication::restoreOverrideCursor();
        imageView->setCursor(Qt::ArrowCursor);
        slideShowStatusLabel->setText("");
        updateStatus(true, "", "MW::slideShow");
        updateStatusBar();
        slideShowAction->setText(tr("Slide Show"));
        /* deleteLater: the show can stop from inside either timer's own timeout
           (nextSlide at an unwrapped end) */
        slideShowTimer->stop();
        slideShowTimer->deleteLater();
        slideShowWaitTimer->stop();
        slideShowWaitTimer->deleteLater();
        slideShowWaitingFirst = false;
        progress->setSuppressed(false);   // end slideshow: allow progress to show again
        // change to ImageCache
        if (G::useImageCache)
            emit setImageCachePosition(dm->currentKey, "MW::slideShow");     // queued
        // nothing the show asked for ahead of itself is wanted any more
        slideShowClearRandomAhead();
        dropQueuedDevPreviews(slideShowDevPreviewAhead);
        slideShowDevPreviewAhead.clear();
        // enable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WindowShortcut);
        // started from the Slide Show module: back to the workflow it was entered from
        leaveSlideShowWorkflow();
        /* Started by S in Develop: back to Develop, the way D enters it (its layout). */
        const bool backToDevelop = slideShowLeftDevelop;
        slideShowLeftDevelop = false;
        if (backToDevelop && G::operationMode != G::OperationMode::Develop
            && operationModeAction && operationModeAction->isEnabled())
            operationModeAction->trigger();
    }
    else {
        // start slideshow
        /*  A SLIDESHOW NEVER DEVELOPS. In Develop the ImageCache reads ahead nothing but
            the current image and decodes raws from the sensor, so slides arrived late or
            not at all and each one paid a demosaic. The show runs in Preview, where the
            cache serves the best preview it has (devPreview, else the embedded JPEG) and
            reads ahead; slideShowBuildAhead makes the devPreviews the screen needs. The
            Slide Show module has already left Develop (invokeSlideShowWorkflow) and
            returns to it through leaveSlideShowWorkflow; S remembers it here. */
        slideShowLeftDevelop = false;
        if (G::operationMode == G::OperationMode::Develop) {
            slideShowLeftDevelop = true;
            setOperationMode(G::OperationMode::Preview);
        }
        slideShowClearRandomAhead();
        slideShowDevPreviewAhead.clear();
        slideCount = 0;
        prevUseImageCache = G::useImageCache;
        QApplication::setOverrideCursor(Qt::BlankCursor);
        imageView->setCursor(Qt::BlankCursor);
        G::isSlideShow = true;
//        isSlideshowPaused = false;
        updateStatusBar();
        QString msg = "<h2>Press " + G::keyHtml("Esc") + " to exit slideshow</h2><p>";
        msg += "Press " + G::keyHtml("H") + " during slideshow for tips"
                      "<p>Starting slideshow";
        msg += "<p>Current settings:<p>";
        msg += "Interval = " + QString::number(slideShowDelay) + " second(s)";
        if (isSlideShowRandom)  msg += "<br>Random selection";
        else msg += "<br>Sequential selection";
        if (isSlideShowWrap) msg += "<br>Wrap at end of slides";
        else msg += "<br>Stop at end of slides";
        msg += "<p>Press " + G::keyHtml("SpaceBar") + " to start slideshow.";
        slideShowStartMsg = msg;

        // No image caching if random slide show
        if (isSlideShowRandom) G::useImageCache = false;
        else G::useImageCache = true;

        // disable main window QAction shortcuts
        QList<QAction*> actions = findChildren<QAction*>();
        for (QAction *a : actions) a->setShortcutContext(Qt::WidgetShortcut);

        if (G::isStressTest) getSubfolders("/users/roryhill/pictures");

        slideShowAction->setText(tr("Stop Slide Show"));
        slideShowFolderStampsValid = false;     // first slide takes a full look
        slideShowTimer = new QTimer(this);
        connect(slideShowTimer, &QTimer::timeout, this, &MW::slideShowTick);
        slideShowWaitTimer = new QTimer(this);
        slideShowWaitTimer->setInterval(kSlideShowWaitPollMs);
        connect(slideShowWaitTimer, &QTimer::timeout, this, &MW::slideShowWaitCheck);

        /*  THE FIRST SLIDE WAITS FOR ITS PICTURE. Look ahead now (pick the random slides,
            queue the devPreviews), and if the first slide is not ready say so --
            "Prefetching" -- until it is, rather than put up a thumbnail-sized preview. */
        slideShowSyncFolders();
        if (isSlideShowRandom) slideShowFillRandomAhead();
        slideShowBuildAhead();
        if (isSlideShowRandom)
            for (const QString &k : std::as_const(slideShowRandomAhead))
                if (!slideShowPrefetchSerial.contains(k)) slideShowPrefetch(k);
        if (slideShowNextReady()) {
            G::popup->showPopup(msg, 3000, true, 0.75, Qt::AlignLeft);
            nextSlide();
            slideShowTimer->start(slideShowDelay * 1000);
        }
        else {
            slideShowWaitingFirst = true;
            G::popup->showPopup(msg + "<p><b>Prefetching the first slide...</b>", 0,
                                true, 0.75, Qt::AlignLeft);
            slideShowWaitClock.start();
            slideShowWaitTimer->start();
        }
    }
}

void MW::nextSlide()
{
    if (G::isLogger) G::log("MW::nextSlide");

    // pick up files added to or removed from the folders since the last slide
    slideShowSyncFolders();
    if (!G::isSlideShow) return;
    if (dm->sf->rowCount() == 0) {
        slideShow();    // every slide is gone: stop
        return;
    }

    slideCount++;
    if (isSlideShowRandom) {
        // push previous image path onto the slideshow history stack
        int row = thumbView->currentIndex().row();
        QString fPath = dm->sf->index(row, 0).data(G::KeyRole).toString();
        slideshowRandomHistoryStack->push(fPath);
        /* The first of the slides picked (and decoded) ahead, if it is still here. Its
           decoded image goes to the loupe first, so loadImage does not decode it again. */
        QString next;
        while (!slideShowRandomAhead.isEmpty() && next.isEmpty()) {
            const QString k = slideShowRandomAhead.takeFirst();
            const QImage im = slideShowPrefetched.take(k);
            slideShowPrefetchedDev.remove(k);
            slideShowPrefetchSerial.remove(k);
            if (k == fPath || dm->proxyRowFromKey(k, "MW::nextSlide") < 0) continue;
            next = k;
            imageView->setSlidePrefetch(k, im);     // null: loadImage decodes it
        }
        if (!next.isEmpty()) sel->setCurrentPath(next);
        else sel->random();
    }
    else {
        if (dm->currentSfRow == dm->sf->rowCount() - 1) {
            if (isSlideShowWrap) sel->first();
            else slideShow();
        }
        else sel->next();
    }

    QString msg = "  Slideshow count:"+ QString::number(slideCount) +
            "  (" + G::keyHtml("press H for slideshow shortcuts", false) + ")";
    updateStatus(true, msg, "MW::nextSlide");

    /* After this slide has painted: top up the random slides picked ahead, and queue
       the devPreviews the next slides need -- before the prefetch decodes, so a slide
       that will get a devPreview is not decoded from its small embedded JPEG first. */
    QTimer::singleShot(50, this, [this] {
        if (!G::isSlideShow) return;
        if (isSlideShowRandom) slideShowFillRandomAhead();
        slideShowBuildAhead();
        if (isSlideShowRandom)
            for (const QString &k : std::as_const(slideShowRandomAhead))
                if (!slideShowPrefetchSerial.contains(k)) slideShowPrefetch(k);
    });
}

void MW::slideShowClearRandomAhead()
{
    slideShowRandomAhead.clear();
    slideShowPrefetched.clear();
    slideShowPrefetchedDev.clear();
    slideShowPrefetchSerial.clear();
    imageView->setSlidePrefetch(QString(), QImage());
}

void MW::slideShowFillRandomAhead()
{
/*
    RANDOM ORDER CACHES kSlideShowAhead SLIDES AHEAD. The ImageCache reads ahead in sort
    order, which is useless when the next slide is anywhere, so it is off in random order.
    Instead the next kSlideShowAhead slides are chosen in advance (slideShowRandomAhead,
    shown in that order by nextSlide), and each is decoded on a pool thread
    (slideShowPrefetch). One slide ahead was not enough: a raw that needs a devPreview
    (a Nikon D2H NEF embeds only a 570 px preview) takes longer than a short interval to
    render, so with five in hand its render has several intervals to land.

    The metadata a far-off row may still lack is read here, on the GUI thread, between
    slides -- not when the slide is due. Only the newly picked slides pay it, normally
    one per slide.
*/
    if (G::isLogger) G::log("MW::slideShowFillRandomAhead");
    const int n = dm->sf->rowCount();
    if (n < 2) return;
    const int want = qMin(kSlideShowAhead, n - 1);
    int tries = 0;
    while (slideShowRandomAhead.count() < want && tries++ < want * 20) {
        const int row = static_cast<int>(QRandomGenerator::global()->bounded(n));
        if (row == dm->currentSfRow) continue;
        const QString key = dm->sf->index(row, 0).data(G::KeyRole).toString();
        if (key.isEmpty() || slideShowRandomAhead.contains(key)) continue;
        const int dmRow = dm->rowFromKey(key);
        if (dmRow < 0) continue;
        if (dm->index(dmRow, G::MetadataStatusColumn).data().toInt() != G::MetaLoaded) {
            QFileInfo fileInfo(key);
            if (metadata->loadImageMetadata(fileInfo, dmRow, dm->instance, true, true,
                                            false, true, "MW::slideShowFillRandomAhead"))
                dm->addMetadataForItem(metadata->m, "MW::slideShowFillRandomAhead");
            /* a failed read still goes in: loadImage tries again, and reports it */
        }
        slideShowRandomAhead << key;
    }
}

void MW::slideShowPrefetch(const QString &key)
{
/*
    Decode one random slide picked ahead, on a pool thread (SlidePreview: the devPreview
    or the embedded JPEG, never a develop), into slideShowPrefetched. Called again for a
    slide whose devPreview has just been built (slideShowDevPreviewBuilt), so the better
    picture replaces the embedded JPEG decoded before it existed; the serial drops a
    decode that a newer one has overtaken.

    Held at most screen size, tagged with the size it stands for (as a reduced
    devPreview is), so five prefetched 45 MP slides do not hold gigabytes. A slide is
    shown fitted, and the loupe presents it at its full size either way.
*/
    if (key.isEmpty()) return;
    const SlidePreview::Job job = SlidePreview::gather(dm, metadata, key);
    if (!job.valid) return;
    const quint64 serial = ++slideShowPrefetchCounter;
    slideShowPrefetchSerial[key] = serial;
    const int maxEdge = slideShowScreenLongEdge();
    using Result = QPair<QImage, bool>;         // the picture, and whether a devPreview
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, key, serial] {
        watcher->deleteLater();
        const Result r = watcher->result();
        if (!G::isSlideShow || !isSlideShowRandom) return;
        if (slideShowPrefetchSerial.value(key) != serial) return;   // superseded or shown
        /* A null image is kept too: it says "decoded, nothing here" (TIFF, HEIC), so
           the slide is ready and loadImage decodes it with Pixmap::load. */
        slideShowPrefetched.insert(key, r.first);
        if (r.second) slideShowPrefetchedDev.insert(key);
        else slideShowPrefetchedDev.remove(key);
    });
    watcher->setFuture(QtConcurrent::run([job, maxEdge] {
        bool fromDev = false;
        QImage im = SlidePreview::decode(job, &fromDev, maxEdge);
        return Result(im, fromDev);
    }));
}

void MW::slideShowDevPreviewBuilt(const QString &fPath)
{
/*
    Called by devPreviewStore when a devPreview is written. A random slide picked ahead
    was probably decoded from its embedded JPEG before the devPreview existed: decode it
    again. Sequential order needs nothing here -- devPreviewStore invalidates the
    ImageCache's decode, and the cache refills from the devPreview.
*/
    if (!G::isSlideShow || !isSlideShowRandom) return;
    if (!slideShowRandomAhead.contains(fPath)) return;
    slideShowPrefetched.remove(fPath);
    slideShowPrefetchedDev.remove(fPath);
    slideShowPrefetch(fPath);
}

void MW::slideShowBuildAhead()
{
/*
    A RAW WHOSE PREVIEW CANNOT FILL THE SCREEN GETS A devPreview. Many cameras embed a
    preview far smaller than the display (1616 px in some Nikons, 1024 px in some DNGs),
    and a slide blown up from one is soft. A devPreview -- the raw's default render, or
    its develop recipe -- is full size, and once it is in the devPreview cache the
    ImageCache serves it instead of the embedded JPEG (ImageDecoder::loadDevPreview).

    So for the next kSlideShowAhead slides (the ones picked ahead in random order) the
    show asks the devPreview builder for one when:

      - the picture shown is the developed one (Show Edits on: Original would not use it),
      - the image can have one (devPreviewBuildKey: edited, or a raw Winnow demosaics),
      - and it is edited (its embedded JPEG is the wrong picture at any size), or its
        embedded preview's long edge is shorter than the screen's, in device pixels.

    The builder renders one at a time and skips what is already cached, so asking every
    slide costs nothing once a folder is built. A slide that arrives before its render
    lands shows the embedded preview; the finished devPreview replaces the cached decode
    (devPreviewStore) for the next time round. Builds the show queued for slides it has
    since passed are withdrawn, so a short interval cannot leave the builder working
    through slides long gone.
*/
    if (G::isLogger) G::log("MW::slideShowBuildAhead");
    if (!G::isSlideShow || !dm || !developProperties) return;
    if (G::previewSource != G::PreviewSource::Developed) return;

    QStringList ahead;
    if (isSlideShowRandom) {
        ahead = slideShowRandomAhead;
    }
    else {
        const int n = dm->sf->rowCount();
        for (int i = 1; i <= kSlideShowAhead && i < n; ++i) {
            int row = dm->currentSfRow + i;
            if (row >= n) {
                if (!isSlideShowWrap) break;
                row -= n;
            }
            ahead << dm->sf->index(row, 0).data(G::KeyRole).toString();
        }
    }

    const int screenLong = slideShowScreenLongEdge();

    QSet<QString> want;
    QStringList build;
    for (const QString &key : std::as_const(ahead)) {
        if (!slideShowWantsDevPreview(key, screenLong)) continue;
        want.insert(key);
        /* Already queued by someone else (a menu or background build): theirs, not ours
           to withdraw later. */
        if (devPreviewBuildQueue.contains(key) && !slideShowDevPreviewAhead.contains(key))
            continue;
        build << key;
    }

    QSet<QString> passed = slideShowDevPreviewAhead;
    passed.subtract(want);
    dropQueuedDevPreviews(passed);
    slideShowDevPreviewAhead = QSet<QString>(build.cbegin(), build.cend());
    if (!build.isEmpty()) buildDevPreviews(build, "slideshow");
}

void MW::slideShowTick()
{
/*
    The interval is up. A SLIDE IS NOT SHOWN BEFORE ITS PICTURE IS READY: if the next
    slide is still decoding, or its devPreview is still rendering, the current slide
    stays up past its time and slideShowWaitCheck advances the moment the next is ready.
    The interval restarts from the slide actually shown, so a waited-for slide still gets
    its full time on screen.
*/
    if (G::isLogger) G::log("MW::slideShowTick");
    if (!G::isSlideShow) return;
    if (slideShowNextReady()) {
        nextSlide();
        return;
    }
    slideShowTimer->stop();
    slideShowWaitClock.start();
    slideShowWaitTimer->start();
}

void MW::slideShowWaitCheck()
{
/*
    Polled every kSlideShowWaitPollMs while a slide waits. Bounded: after
    kSlideShowMaxWaitMs the show moves on with whatever the slide has (a decode that
    never lands, a memory-throttled cache), so it can never hang on one image.
*/
    if (!G::isSlideShow) { slideShowWaitTimer->stop(); return; }
    const bool timedOut = slideShowWaitClock.elapsed() > kSlideShowMaxWaitMs;
    if (!timedOut && !slideShowNextReady()) return;
    if (timedOut)
        G::issueDedup("Warning", "Slideshow: the next slide was not ready after " +
                      QString::number(kSlideShowMaxWaitMs / 1000) + " s, shown as is",
                      "MW::slideShowWaitCheck", -1, slideShowNextKey());
    slideShowWaitTimer->stop();
    if (slideShowWaitingFirst) {
        slideShowWaitingFirst = false;
        G::popup->reset();
        G::popup->showPopup(slideShowStartMsg, 3000, true, 0.75, Qt::AlignLeft);
    }
    nextSlide();
    if (G::isSlideShow) slideShowTimer->start(slideShowDelay * 1000);
}

bool MW::slideShowRunning() const
{
    // not paused: either counting down the interval or waiting on the next slide
    return G::isSlideShow && (slideShowTimer->isActive() || slideShowWaitTimer->isActive());
}

QString MW::slideShowNextKey()
{
/*
    The slide nextSlide will show, without moving to it. Mirrors nextSlide: the first
    valid random pick, or the next row (row 0 when wrapping; empty at an unwrapped end,
    where nextSlide stops the show).
*/
    const int n = dm->sf->rowCount();
    if (n == 0) return QString();
    if (isSlideShowRandom) {
        const QString cur = dm->currentKey;
        for (const QString &k : std::as_const(slideShowRandomAhead))
            if (k != cur && dm->proxyRowFromKey(k, "MW::slideShowNextKey") >= 0) return k;
        return QString();
    }
    int row = dm->currentSfRow + 1;
    if (row >= n) {
        if (!isSlideShowWrap) return QString();
        row = 0;
    }
    return dm->sf->index(row, 0).data(G::KeyRole).toString();
}

bool MW::slideShowNextReady()
{
/*
    Whether the next slide's BEST picture is in hand:

      - a devPreview the show wants for it (slideShowWantsDevPreview) is cached, or is
        no longer coming (not queued, not rendering, not in the builder's cache check --
        so a failed render does not hold the show);
      - and the picture is decoded: in random order the prefetch has landed, from the
        devPreview when there is one; in sequential order the ImageCache holds it (a
        devPreview written for it drops the cache's earlier decode, devPreviewStore, so
        a hit after that is the devPreview), or its decode has been tried and failed.

    Anything nextSlide would not show from here (end of an unwrapped show, an empty
    random pick that falls back to Selection::random) is ready.
*/
    const QString key = slideShowNextKey();
    if (key.isEmpty()) return true;
    const int row = dm->rowFromKey(key);
    if (row < 0) return true;
    if (metadata->videoFormats.contains(QFileInfo(key).suffix().toLower())) return true;

    const int screenLong = slideShowScreenLongEdge();
    bool devCached = false;
    if (slideShowWantsDevPreview(key, screenLong)) {
        const QString bk = devPreviewBuildKey(key);
        devCached = DevPreviewCache::instance().contains(key, bk.toLatin1(), true);
        const bool coming = devPreviewBuildQueue.contains(key)
                            || devPreviewBuildCurrent == key
                            || devPreviewHopPaths.contains(key);
        if (!devCached && coming) return false;
    }

    if (isSlideShowRandom) {
        if (!slideShowPrefetched.contains(key)) {
            // never asked (picked by the timer in the same tick): ask now
            if (!slideShowPrefetchSerial.contains(key)) slideShowPrefetch(key);
            return false;
        }
        /* Decoded from the embedded JPEG before the devPreview landed: the re-decode
           slideShowDevPreviewBuilt asked for is on its way. */
        if (devCached && !slideShowPrefetchedDev.contains(key)) return false;
        return true;
    }

    if (icd->contains(key)) return true;
    // tried and not caching: failed, so loadImage decodes (or reports) it itself
    return dm->index(row, G::AttemptsColumn).data().toInt() > 0
           && !dm->index(row, G::IsCachingColumn).data().toBool();
}

int MW::slideShowScreenLongEdge() const
{
    QScreen *scr = imageView->screen();
    const QSize sz = scr ? scr->size() * scr->devicePixelRatio() : QSize();
    return qMax(sz.width(), sz.height());
}

bool MW::slideShowWantsDevPreview(const QString &key, int screenLong)
{
/*
    The rule for asking the builder for a slide's devPreview (see slideShowBuildAhead):
    Show Edits on, an image devPreviewBuildKey can key, and either edited (its embedded
    JPEG is the wrong picture) or with an embedded preview shorter than the screen.
*/
    if (G::previewSource != G::PreviewSource::Developed || !developProperties) return false;
    const int row = dm->rowFromKey(key);
    if (row < 0) return false;
    if (devPreviewBuildKey(key).isEmpty()) return false;
    if (dm->index(row, G::DevelopColumn).data().toBool()) return true;
    const int previewLong = qMax(dm->index(row, G::WidthOrigPreviewColumn).data().toInt(),
                                 dm->index(row, G::HeightOrigPreviewColumn).data().toInt());
    return previewLong < screenLong;
}

void MW::slideShowSyncFolders()
{
/*
    Called by MW::nextSlide before every slide change, so a slideshow keeps up with
    folders that are changing under it (a camera tethered into one, files deleted in
    Finder / Explorer). The reconcile itself is MW::refresh -- the same added / removed
    / modified diff and on-the-fly model change as File > Refresh, keeping the filters.

    CHEAP UNTIL SOMETHING CHANGES. MW::refresh lists every folder, stats every row and
    bumps the model instance (which drops in-flight ImageCache decodes) even when it
    finds nothing, which is too much to pay every few seconds on a large load. Adding,
    removing or renaming a file updates its DIRECTORY's modified time on both APFS and
    NTFS, so each slide stats only the loaded folders and refreshes only when one moved.

    - The first slide (and a different folder set, per dm->folderListGen) does the full
      check, catching anything that changed between the load and the show.
    - A folder modified in the last few seconds keeps getting the full check: exFAT /
      FAT stamps have 2 s resolution, so a second add inside the same tick would leave
      the stamp unchanged.
    - Catalog scope is skipped: its rows are a query result, not a folder listing.
    - Not while a load is filling the model (isModifyingDatamodel / isLoadRunning).
    - Only folders already in the load are watched; a NEW subfolder of a recursive load
      is not picked up (its parent's stamp moves, but the listing is per folder).
*/
    if (G::isLogger) G::log("MW::slideShowSyncFolders");
    if (G::scope == G::Scope::Catalog) return;
    if (G::isModifyingDatamodel || G::isLoadRunning) return;
    if (dm->folderList.isEmpty()) return;

    const quint64 gen = dm->folderListGen.load();
    bool changed = false;
    if (!slideShowFolderStampsValid || gen != slideShowFolderGen) {
        slideShowFolderStamps.clear();
        slideShowFolderGen = gen;
        slideShowFolderStampsValid = true;
        changed = true;
    }

    const QDateTime now = QDateTime::currentDateTime();
    for (const QString &folder : std::as_const(dm->folderList)) {
        // a fresh QFileInfo each time: it caches, and a stale stamp would hide a change
        const QDateTime t = QFileInfo(folder).lastModified();
        auto it = slideShowFolderStamps.find(folder);
        if (it == slideShowFolderStamps.end()) {
            slideShowFolderStamps.insert(folder, t);
            changed = true;
            continue;
        }
        if (*it != t) {
            *it = t;
            changed = true;
        }
        else if (t.isValid() && t.msecsTo(now) < 3000) changed = true;
    }
    if (!changed) return;

    QStringList added, removed, modified;
    if (!dm->sourceModified(added, removed, modified)) return;
    refresh();
}

void MW::prevRandomSlide()
{
    if (G::isLogger) G::log("MW::prevRandomSlide");
    if (slideshowRandomHistoryStack->isEmpty()) {
        G::popup->showPopup("End of random slide history");
        return;
    }
//    isSlideshowPaused = true;
    QString prevPath = slideshowRandomHistoryStack->pop();
    sel->setCurrentPath(prevPath);
    updateStatus(false,
                 "Slideshow random history."
                 "  Press <font color=\"white\"><b>Spacebar</b></font> to continue slideshow, "
                 "press <font color=\"white\"><b>Esc</b></font> to quit slideshow."
                 , "MW::prevRandomSlide");
    // hide popup if showing
    G::popup->reset();
}

void MW::slideShowResetDelay()
{
    if (G::isLogger) G::log("MW::slideShowResetDelay");
    slideShowTimer->setInterval(slideShowDelay * 1000);
}

void MW::slideShowResetSequence()
{
/*
    Called from MW::keyReleaseEvent when R is pressed and isSlideShow == true.
    The slideshow is toggled between sequential and random progress.
*/
    if (G::isLogger) G::log("MW::slideShowResetSequence");
    QString msg = "Setting slideshow progress to ";
    /* The ImageCache follows the order, as it does at the start of a show: off in random
       order (slideShowFillRandomAhead reads kSlideShowAhead slides ahead instead), on in sequential,
       where it reads ahead from the current slide. Toggling used to leave it as the show
       started, so a show begun random and switched to sequential cached nothing. */
    slideShowClearRandomAhead();
    if (isSlideShowRandom) {
        msg += "random";
        G::useImageCache = false;
        progress->setSuppressed(true);    // no caching in random mode: hide progress
    }
    else {
        msg = msg + "sequential";
        G::useImageCache = true;
        emit setImageCachePosition(dm->currentKey, "MW::slideShowResetSequence");
        progress->setSuppressed(false);   // let progress show per its row content
    }
    G::popup->showPopup(msg);
}

void MW::slideshowHelpMsg()
{
    if (G::isLogger) G::log("MW::slideshowHelpMsg");
    QString selection;
    if (isSlideShowRandom)  selection = "Random selection";
    else selection = "Sequential selection";
    QString wrap;
    if (isSlideShowWrap)  wrap = "Wrap at end of slides";
    else wrap = "Stop at end of slides";
    QString msg =
        "<p><b>Slideshow Shortcuts:</b><br/></p>"
        "<table border=\"0\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px;\" cellspacing=\"2\" cellpadding=\"0\">"
        "<tr><td width=\"120\">" + G::keyHtml("Esc") + "</td><td>Exit slideshow</td></tr>"
        "<tr><td>" + G::keyHtml("  S       ") + "</td><td>Exit slideshow</td></tr>"
        "<tr><td>" + G::keyHtml("  W       ") + "</td><td>Toggle wrapping on and off</td></tr>"
        "<tr><td>" + G::keyHtml("  R       ") + "</td><td>Toggle random vs sequential slide selection</td></tr>"
        //"<tr><td><font color=\"red\"><b>Backspace </b></font></td><td>Go back to a previous random slide</td></tr>"
        "<tr><td>" + G::keyHtml("Spacebar  ") + "</td><td>Pause/Continue slideshow</td></tr>"
        "<tr><td>" + G::keyHtml("  H       ") + "</td><td>Show this popup message</td></tr>"
        "</table>"
        "<p>Change the slideshow interval.  Go to preferences to set other interval.</p>"
        "<table border=\"0\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px;\" cellspacing=\"2\" cellpadding=\"0\">"
        "<tr><td width=\"120\">" + G::keyHtml("1") + "</td><td>1 second</td></tr>"
        "<tr><td>" + G::keyHtml("  2  ") + "</td><td>2 seconds</td></tr>"
        "<tr><td>" + G::keyHtml("  3  ") + "</td><td>3 seconds</td></tr>"
        "<tr><td>" + G::keyHtml("  4  ") + "</td><td>5 seconds</td></tr>"
        "<tr><td>" + G::keyHtml("  5  ") + "</td><td>10 seconds</td></tr>"
        "<tr><td>" + G::keyHtml("  6  ") + "</td><td>30 seconds</td></tr>"
        "<tr><td>" + G::keyHtml("  7  ") + "</td><td>1 minute</td></tr>"
        "<tr><td>" + G::keyHtml("  8  ") + "</td><td>3 minutes</td></tr>"
        "<tr><td>" + G::keyHtml("  9  ") + "</td><td>10 minutes</td></tr>"
        "</table>"
        "<p>Current settings:<p>"
        "<ul style=\"line-height:50%; list-style-type:none;\""
        "<li>Interval  = "  + QString::number(slideShowDelay) + " seconds</li>"
        "<li>Selection = " + selection + "</li>"
        "<li>Wrap      = " + wrap + "</li>"
        "</ul><p><p>"
        "Press " + G::keyHtml("Space Bar") + " continue slideshow and close this message";
    G::popup->showPopup(msg, 0, true, 1.0, Qt::AlignLeft);
}


/* ---------------------------------------------------------------------------
   The in-slideshow keys.

   One slot per key, so MW::keyReleaseEvent and the View > Slide Show menu items run the
   same code instead of two copies of it.  Each is a no-op unless a slideshow is running
   (slideShowRunning: counting down, or waiting on the next slide). The timers are
   deleted (and not nulled) when the slideshow ends, so slideShowRunning tests
   G::isSlideShow before it touches them.
   --------------------------------------------------------------------------- */

void MW::slideShowNext()
{
    if (G::isLogger) G::log("MW::slideShowNext");
    if (!slideShowRunning()) return;
    // asked for: shown now, ready or not, and the interval restarts from it
    slideShowWaitTimer->stop();
    nextSlide();
    if (G::isSlideShow) slideShowTimer->start(slideShowDelay * 1000);
}

void MW::slideShowPrevRandom()
{
    if (G::isLogger) G::log("MW::slideShowPrevRandom");
    if (!slideShowRunning()) return;
    prevRandomSlide();
}

void MW::slideShowPauseOrContinue()
{
/*
    Space pauses a running slideshow and resumes a paused one.  The paused branch also
    clears the help popup, the way the key handler does when it wakes a paused slideshow.
*/
    if (G::isLogger) G::log("MW::slideShowPauseOrContinue");
    if (!G::isSlideShow) return;
    if (slideShowRunning()) {
        slideShowTimer->stop();
        slideShowWaitTimer->stop();
        slideShowWaitingFirst = false;
        G::popup->showPopup("Slideshow is paused", 0);
    }
    else {
        if (isSlideShowHelpVisible) {
            G::popup->reset();
            isSlideShowHelpVisible = false;
        }
        G::popup->showPopup("Slideshow is active");
        slideShowTick();        // the next slide when it is ready, waiting if not
        if (G::isSlideShow && !slideShowWaitTimer->isActive())
            slideShowTimer->start(slideShowDelay * 1000);
    }
}

void MW::slideShowToggleWrap()
{
    if (G::isLogger) G::log("MW::slideShowToggleWrap");
    if (!slideShowRunning()) return;
    isSlideShowWrap = !isSlideShowWrap;
    isSlideShowHelpVisible = true;
    QString msg;
    if (isSlideShowWrap) msg = "Slide wrapping is on.";
    else msg = "Slide wrapping is off.";
    G::popup->showPopup(msg);
}

void MW::slideShowToggleRandom()
{
    if (G::isLogger) G::log("MW::slideShowToggleRandom");
    if (!slideShowRunning()) return;
    isSlideShowRandom = !isSlideShowRandom;
    slideShowResetSequence();
    QString msg;
    if (isSlideShowRandom) msg = "Random selection enabled.";
    else msg = "Sequential selection enabled.";
    G::popup->showPopup(msg);
}

void MW::slideShowKeysHelp()
{
    if (G::isLogger) G::log("MW::slideShowKeysHelp");
    if (!slideShowRunning()) return;
    slideShowTimer->stop();
    slideShowWaitTimer->stop();
    slideshowHelpMsg();
}

void MW::slideShowSetDelayFromAction(QAction *action)
{
/*
    The interval actions carry their seconds in QAction::data, so the same slot serves all
    nine.  Keys 1-9 reach it through MW::keyReleaseEvent.
*/
    if (G::isLogger) G::log("MW::slideShowSetDelayFromAction");
    if (action == nullptr) return;
    if (!slideShowRunning()) return;
    slideShowDelay = action->data().toInt();
    slideShowResetDelay();
    QString msg = "Slideshow interval set to " + QString::number(slideShowDelay) + " seconds.";
    G::popup->showPopup(msg);
}

void MW::syncSlideShowMenuEnabled()
{
/*
    Called as View > Slide Show opens.  Everything below Start / Stop only works while a
    slideshow is running -- outside one those keys belong to other actions entirely (X is
    Reject, W is New Workspace, R is unbound, Space is zoom toggle) -- so an enabled item
    would advertise a shortcut that does something else.  Same reasoning, and the same
    aboutToShow treatment, as MW::syncDevelopMenuEnabled.
*/
    if (G::isLogger) G::log("MW::syncSlideShowMenuEnabled");
    const bool running = G::isSlideShow;

    const QList<QAction *> runOnly {
        slideShowNextAction, slideShowPrevRandomAction, slideShowPauseAction,
        slideShowWrapAction, slideShowRandomAction, slideShowKeysAction
    };
    for (QAction *a : runOnly) if (a) a->setEnabled(running);
    if (slideShowIntervalMenu) slideShowIntervalMenu->setEnabled(running);

    // Reflect live state for the toggles
    if (slideShowWrapAction) {
        QSignalBlocker blocker(slideShowWrapAction);
        slideShowWrapAction->setChecked(isSlideShowWrap);
    }
    if (slideShowRandomAction) {
        QSignalBlocker blocker(slideShowRandomAction);
        slideShowRandomAction->setChecked(isSlideShowRandom);
    }
    for (QAction *a : slideShowIntervalActions) {
        QSignalBlocker blocker(a);
        a->setChecked(a->data().toInt() == slideShowDelay);
    }
}
