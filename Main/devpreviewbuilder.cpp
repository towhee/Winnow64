#include "Main/mainwindow.h"
#include "Cache/devpreviewcache.h"
#include "ImageFormats/Raw/rawformat.h"

/*
    BUILDING devPreviews FOR IMAGES THAT ARE NOT OPEN IN DEVELOP

    A devPreview is normally a byproduct: the develop render already happened, so writing
    one is a scale plus a JPEG encode. That covers the image on screen and nothing else.
    Every image edited in an earlier session, and every target of a multi-image paste, has
    a recipe but no render -- so no preview, so the grid shows the camera thumbnail
    and the loupe has to decode the raw.

    Closing that gap means DECODING AND RENDERING an image purely to cache it, which is
    exactly what the byproduct rule exists to avoid. So it never happens unasked: either
    the user runs Develop > Build Developed Previews, or they turn on "Build developed
    previews in background" and accept the cost.

    TWO POPULATIONS ARE BUILT, and the second is much the larger:

      edited      an image with a develop recipe, keyed on the hash of that recipe. This is
                  the original purpose above -- usually a handful of images per folder.
      unedited    ANY raw Winnow has a sensor decoder for, keyed on the renderer instead
                  (Metadata::defaultRenderKey). What gets cached is the DEFAULT RENDER: the
                  pipeline run with identity adjustments, which is what the user would see
                  on entering Develop without touching a slider.

    The second exists purely for browsing speed. With it cached, the loupe serves a raw from
    a JPEG decode instead of a demosaic (ImageDecoder::loadDevPreview), so an unedited raw
    folder browses like a folder of JPEGs -- and it shows WINNOW's rendering of the sensor
    data rather than the camera's embedded JPEG. The cost is paid once, up front, and it is
    steep: a thousand-raw folder is a thousand full decodes and composites, run one at a
    time, so hours rather than seconds. That trade -- a long one-off build for permanently
    fast browsing -- is the whole point, and it is why the preference is off by default.

    ONLY THE LARGE TIER FOR AN UNEDITED RAW. The 256px grid thumbnail lives in the image's
    XMP sidecar, and an image the user never edited should not gain a sidecar as a side
    effect of a cache fill. The grid keeps the camera-embedded thumbnail, which was already
    read during the metadata pass and is no slower. See devPreviewStore.

    ONE AT A TIME, ON THE EXISTING ENGINE. The work is done by MW::developPixelSource --
    the exporter's render path -- which already stages a full-resolution render correctly:
    the GUI thread owns the datamodel reads, the stored-recipe capture, the orientation
    and the mask prerequisites, while the decode and the composite run on
    developRenderPool. Reusing it means the builder cannot drift from what an export or a
    real develop render produces.

    PROGRESS IS A STATUS-BAR ROW, not a popup. A build is minutes of rendering the user is
    not waiting on -- they carry on browsing -- so it reports where the other background
    work reports (Metadata, Raw Denoise, Demosaicing), not under a popup parked over the
    image. Only a run the user explicitly asked for says anything when it ends.

    Images are processed strictly sequentially -- the next one starts when the previous
    one's render returns. That is what keeps the builder inside the raw-decoder
    concurrency budget (see ImageCache::rawDecodeLimit and the memory-overrun history
    behind it): one in-flight scene-linear decode, whatever the folder size. It also
    means a long build never competes with the image the user is looking at.
*/

void MW::buildDevPreviews(const QStringList &paths, const QString &src)
{
/*
    Queue devPreviews for paths that should have one and do not. Re-running after a partial
    build is free: everything already done is filtered out, so the queue only ever holds
    real work.

    TWO PASSES, ON TWO THREADS, because the two halves of "does this need building?" have
    very different costs:

      GUI thread   which key the image should carry (devPreviewBuildKey). Model reads and,
                   for the few edited images, a sidecar read. No I/O for the rest.
      worker       whether the cache already holds that key -- one SQLite query AND one
                   stat() per path (DevPreviewCache::contains).

    The second pass used to run here. That was survivable when only edited images were
    queued (a handful), but a background build now offers every raw in the folder: on the
    1000-image folders this feature exists for, that is a thousand queries and a thousand
    stats on the GUI thread, at the moment the folder finishes loading. Off-thread it costs
    the user nothing, and the build starting a beat later is invisible against a run
    measured in hours.
*/
    if (G::isLogger) G::log("MW::buildDevPreviews", src);
    if (!developProperties || !dm) return;

    QVector<QPair<QString, QString>> candidates;     // path + the key it should carry
    candidates.reserve(paths.count());
    for (const QString &fPath : paths) {
        if (fPath.isEmpty()) continue;
        if (devPreviewBuildQueue.contains(fPath)) continue;
        if (fPath == devPreviewBuildCurrent) continue;
        const QString key = devPreviewBuildKey(fPath);
        if (key.isEmpty()) continue;                 // nothing to depict for this file
        candidates.append({fPath, key});
    }
    if (candidates.isEmpty()) {
        if (src == "menu" && G::popup)
            G::popup->showPopup("Every image already has a current developed preview.",
                                2000);
        return;
    }

    /* Guard the hand-back: a folder change while this pass runs makes every path in it
       irrelevant, and MW::stop has already cleared the queue by then. */
    const int instance = dm->instance.load();
    QThreadPool::globalInstance()->start([this, candidates, src, instance]() {
        QStringList need;
        for (const auto &c : candidates) {
            if (G::stop) return;
            if (!DevPreviewCache::instance().contains(c.first, c.second.toLatin1()))
                need << c.first;
        }
        QMetaObject::invokeMethod(this, [this, need, src, instance]() {
            startDevPreviewBuild(need, src, instance);
        });
    });
}

void MW::startDevPreviewBuild(const QStringList &paths, const QString &src, int instance)
{
/*
    GUI-thread continuation of buildDevPreviews: everything here needs the filtered list
    that the worker just produced.
*/
    if (G::isLogger) G::log("MW::startDevPreviewBuild", src);
    if (!dm || dm->instance.load() != instance) return;   // different folder now

    if (paths.isEmpty()) {
        if (src == "menu" && G::popup)
            G::popup->showPopup("Every image already has a current developed preview.",
                                2000);
        return;
    }
    for (const QString &fPath : paths) {
        if (devPreviewBuildQueue.contains(fPath)) continue;
        if (fPath == devPreviewBuildCurrent) continue;
        devPreviewBuildQueue.append(fPath);
    }
    if (devPreviewBuildQueue.isEmpty()) return;

    devPreviewBuildTotal = devPreviewBuildQueue.count() + devPreviewBuildDone;
    devPreviewBuildFromMenu = (src == "menu");
    /* Reveal the row EMPTY. updateProgress paints FromStart, so calling it here with
       item 0 would fill the first cell before anything has been rendered (and the whole
       bar when there is one image). devPreviewBuildNext paints each item as it starts. */
    if (progress) progress->showRow(progressDevPreviewRow, true);
    devPreviewBuildNext();
}

QString MW::devPreviewBuildKey(const QString &fPath) const
{
/*
    The devPreview key fPath SHOULD have, or empty when it should have none. Two kinds:

      edited      hash of the develop recipe (Metadata::devPreviewKey). Keyed on the recipe
                  rather than on the key stored in the sidecar, so an image edited by another
                  Winnow instance or on another machine is picked up too.
      unedited    a raw with a sensor decoder gets the DEFAULT RENDER key -- the image put
                  through the pipeline with identity adjustments. Caching that is what makes
                  browsing an unedited raw folder as fast as browsing JPEGs, since the loupe
                  then never demosaics.

    Anything else -- a JPEG, a raw with no sensor decoder -- returns empty. For those the
    file on disk already IS the default render, so a preview would be a re-encode of it: pure
    cost, no gain.

    The DevelopColumn is consulted rather than developBlobFor for the unedited case on
    purpose. developBlobFor -> stackFor reads the sidecar on first touch, and a background
    build over a 1000-raw folder would turn that into 1000 synchronous sidecar reads on the
    GUI thread. The column was filled from the same sidecars during the metadata read.
*/
    if (fPath.isEmpty() || !developProperties) return QString();

    const int row = dm ? dm->rowFromPath(fPath) : -1;
    const bool edited = row >= 0 &&
                        dm->index(row, G::DevelopColumn).data().toBool();

    /* Only an edited image pays the sidecar read. A recipe reset to identity reads back
       empty here and correctly falls through to the default render. */
    const QString blob = edited ? developProperties->developBlobFor(fPath) : QString();
    if (!blob.isEmpty()) return Metadata::devPreviewKey(blob);

    if (!RawFormat::HasSensorDecoder(QFileInfo(fPath).suffix().toLower()))
        return QString();
    return Metadata::defaultRenderKey();
}

void MW::devPreviewBuildNext()
{
/*
    Render the next queued image. Runs one at a time; developPixelSource calls back on the
    GUI thread when the render lands, and that callback re-enters here.
*/
    if (G::isLogger) G::log("MW::devPreviewBuildNext");

    if (devPreviewBuildCancelled || devPreviewBuildQueue.isEmpty()) {
        devPreviewBuildFinish();
        return;
    }
    /* Developing an image is a live edit session on the same path-keyed caches this
       render would touch, and the user's own render must win. Wait, don't interleave. */
    if (G::operationMode == G::OperationMode::Develop) {
        devPreviewBuildFinish("paused while you are in Develop mode");
        return;
    }

    devPreviewBuildCurrent = devPreviewBuildQueue.takeFirst();
    const QString fPath = devPreviewBuildCurrent;
    /* The key this render is ABOUT to depict, captured before it starts. devPreviewStore
       re-derives it on the way out and writes only if the two agree, so an edit made while
       the image rendered cannot stamp the pre-edit pixels with the post-edit key. */
    const QString expectKey = devPreviewBuildKey(fPath);

    updateDevPreviewBuildProgress();

    /* 8-bit sRGB: a devPreview is a JPEG for the screen, not an export master. */
    developPixelSource(fPath, /*want16Bit*/false, OutputTransform::Space::sRGB,
        [this, fPath, expectKey](bool ok, const QImage &out) {
            if (ok && !out.isNull()) devPreviewStore(fPath, out, expectKey);
            ++devPreviewBuildDone;
            devPreviewBuildCurrent.clear();
            devPreviewBuildNext();
        });
}

void MW::devPreviewStore(const QString &fPath, const QImage &full,
                         const QString &expectKey)
{
/*
    Write the freshly rendered full-resolution image to the preview cache, and -- for an
    EDITED image only -- the 256px thumbnail into its XMP sidecar.

    This deliberately mirrors the provider lambda in initialize.cpp rather than calling it
    -- the provider serves the image on screen and reads developFrame / developFullFrame,
    neither of which describes an image the builder just rendered off-thread.

    THE KEY IS RE-DERIVED HERE and checked against the one captured when the render started.
    A render takes seconds and the user may have edited the image, or switched raw engine, in
    between; either would make the pixels in hand depict something other than what the key
    now says. On a mismatch the write is skipped and the image simply misses again -- it will
    be rebuilt, correctly, next time.

    NO SIDECAR FOR AN UNEDITED RAW. Its preview is the default render, which the user never
    asked for by editing anything, so creating an XMP file beside it would be writing to their
    library as a side effect of a cache fill. The grid keeps the camera-embedded thumbnail --
    already read during the metadata pass, and just as fast. Only the large tier is cached,
    which is the tier that saves the demosaic.
*/
    if (G::isLogger) G::log("MW::devPreviewStore");
    if (!developProperties) return;

    const QString key = devPreviewBuildKey(fPath);
    if (key.isEmpty() || key != expectKey) return;   // moved under us; drop this render

    /* Which TIER this render belongs to, decided from the recipe itself rather than from
       how devPreviewBuildKey happened to answer. A non-empty recipe whose hash is not the
       key we are about to write under means the two disagree -- the develop badge and the
       stack cache are out of step -- and writing either tier would record a picture under a
       description it does not match. Drop it; the next pass rebuilds from a settled state. */
    const QString blob = developProperties->developBlobFor(fPath);
    const QString recipeKey = Metadata::devPreviewKey(blob);   // empty when blob is empty
    if (!recipeKey.isEmpty() && recipeKey != key) return;
    const bool edited = !recipeKey.isEmpty();

    auto encode = [](const QImage &im, int quality, QByteArray &out) {
        QBuffer buf(&out);
        if (!buf.open(QIODevice::WriteOnly)) return false;
        return im.save(&buf, "JPG", quality);
    };

    /* Thumbnail tier: a 256 px grid icon in the sidecar, fixed at 85. It is deliberately
       NOT governed by the "Developed preview quality" preference -- that setting is about
       what the loupe shows at 100%, and a few KB of icon is not where disk is spent. */
    QByteArray thumbJpg;
    QImage thumb;
    if (edited) {
        thumb = full.scaled(G::maxIconSize, G::maxIconSize,
                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (!encode(thumb, 85, thumbJpg)) thumbJpg.clear();
    }

    QByteArray previewJpg;
    QImage preview = full;
    const int cap = G::devPreviewMaxEdge;
    if (cap > 0 && qMax(preview.width(), preview.height()) > cap)
        preview = preview.scaled(cap, cap, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!encode(preview, G::devPreviewQuality, previewJpg)) previewJpg.clear();

    if (thumbJpg.isEmpty() && previewJpg.isEmpty()) return;

    /* Sidecar first: writeDevelopSidecar writes the recipe, the thumbnail and the key in
       ONE pass, so the three can never disagree. The recipe is passed through
       unchanged -- this is not a recipe edit. */
    if (!thumbJpg.isEmpty())
        Metadata::writeDevelopSidecar(fPath, blob, thumbJpg.toBase64());
    if (!previewJpg.isEmpty())
        DevPreviewCache::instance().put(fPath, key.toLatin1(), previewJpg);

    if (edited) {
        /* Bring the grid (and the image cache) into line, exactly as a flushed edit does. */
        devPreviewUpdated(fPath, thumbJpg.isEmpty() ? QImage() : thumb);
        return;
    }

    /* DEFAULT RENDER: devPreviewUpdated would be actively harmful here. It is written for an
       edit, so a null thumbnail means "the icon on screen is now stale" and it clears the
       row, tells MetaRead to re-arm (invalidateLoadedIcons) and calls reloadIconChunk. None
       of that is true of an unedited raw -- its camera thumbnail is correct and unchanged --
       and doing it once per image through a thousand-image build would thrash the grid for
       the whole run.

       The one thing worth doing is dropping the full-size image cached for this path: it was
       decoded from the camera JPEG, and the next visit should serve the devPreview just
       written instead. Skipped for the image ON SCREEN, whose loupe pixmap is that cached
       decode -- it picks the preview up on the next visit rather than blinking now. */
    if (icd && dm && fPath != dm->currentFilePath) icd->remove(fPath);
}

void MW::updateDevPreviewBuildProgress()
{
/*
    Paint the status-bar row. devPreviewBuildDone is the count COMPLETED, which is also
    the 0-based index of the image now rendering, so the bar includes the one in flight --
    the same reading as the other sequential rows (Raw Denoise, Demosaicing).
*/
    if (!devPreviewBuildTotal || !progress) return;
    progress->updateProgress(progressDevPreviewRow, devPreviewBuildDone,
                             devPreviewBuildTotal);
}

void MW::devPreviewBuildFinish(const QString &reason)
{
/*
    End the run, whether it completed, was cancelled or was pre-empted by Develop mode.
    Anything still queued is DISCARDED rather than remembered: the queue is rebuilt from
    what is actually missing whenever the command runs again, so a stale queue could only
    be wrong.
*/
    if (G::isLogger) G::log("MW::devPreviewBuildFinish", reason);

    const int done = devPreviewBuildDone;
    const bool wasRunning = devPreviewBuildTotal > 0;

    devPreviewBuildQueue.clear();
    devPreviewBuildCurrent.clear();
    devPreviewBuildTotal = 0;
    devPreviewBuildDone = 0;
    devPreviewBuildCancelled = false;

    const bool fromMenu = devPreviewBuildFromMenu;
    devPreviewBuildFromMenu = false;

    /* Clear the bar and hide the row. Done unconditionally: a run that never started
       still leaves nothing behind, and clearProgress on an already-hidden row is a
       no-op. */
    if (progress) progress->clearProgress(progressDevPreviewRow);

    if (!wasRunning) return;

    /* Only a run the user asked for reports. A background build is meant to be
       unnoticed -- the row said it was happening and the grid shows the result. */
    if (!fromMenu || !G::popup) return;
    if (!reason.isEmpty())
        G::popup->showPopup(QString("Developed previews: %1 built, %2.")
                                .arg(done).arg(reason), 3000);
    else if (done > 0)
        G::popup->showPopup(QString("Built %1 developed preview%2.")
                                .arg(done).arg(done == 1 ? "" : "s"), 2000);
}

void MW::cancelDevPreviewBuild()
{
/*
    Stop after the render in flight returns. The render itself is not abortable -- it is
    inside developCompositeStack on a worker -- so the flag is read by devPreviewBuildNext
    when that render calls back.
*/
    if (G::isLogger) G::log("MW::cancelDevPreviewBuild");
    if (devPreviewBuildTotal == 0) return;
    devPreviewBuildCancelled = true;
}

void MW::buildDevPreviewsForSelection()
{
/*
    Develop > Build Developed Previews. Acts on the selection, or on the whole folder when
    fewer than two images are selected -- selecting one image and asking to build previews
    almost always means "this folder", not "this one image".
*/
    if (G::isLogger) G::log("MW::buildDevPreviewsForSelection");
    if (!dm || dm->rowCount() == 0) return;

    QStringList paths;
    const QModelIndexList sel = dm->selectionModel->selectedRows();
    if (sel.count() > 1) {
        for (const QModelIndex &idx : sel)
            paths << idx.data(G::PathRole).toString();
    }
    else {
        for (int row = 0; row < dm->sf->rowCount(); ++row)
            paths << dm->sf->index(row, 0).data(G::PathRole).toString();
    }
    buildDevPreviews(paths, "menu");
}

void MW::queueBackgroundDevPreviewBuild()
{
/*
    Called once per folder load when the preference is on. Queues two populations:

      o every EDITED image with no current preview -- the original purpose. Cheap to find
        (G::DevelopColumn came from the sidecar during the metadata read) and usually a
        handful of images.
      o every RAW with a sensor decoder, edited or not, so its DEFAULT RENDER is cached and
        the loupe never has to demosaic it again.

    The second is a different order of magnitude: a thousand-raw folder is a thousand full
    sensor decodes and composites, run strictly one at a time, so hours rather than seconds.
    That is the deliberate trade -- a long one-off cost for a folder that browses like JPEGs
    afterwards. It is why the work stays off the GUI thread, yields to Develop mode, and is
    abandoned wholesale when the user leaves the folder (MW::stop -> cancelDevPreviewBuild).

    Rows already holding a current preview are filtered out downstream by buildDevPreviews
    (off the GUI thread), so re-opening a folder that finished building queues nothing.
*/
    if (G::isLogger) G::log("MW::queueBackgroundDevPreviewBuild");
    if (!G::buildDevPreviewsInBackground) return;
    if (!dm || dm->rowCount() == 0) return;

    QStringList paths;
    for (int row = 0; row < dm->rowCount(); ++row) {
        const QString fPath =
            dm->index(row, G::PathColumn).data(G::PathRole).toString();
        if (fPath.isEmpty()) continue;
        const bool edited = dm->index(row, G::DevelopColumn).data().toBool();
        /* Extension test only -- no file is opened here. buildDevPreviews makes the real
           (and more expensive) per-path decision, on a worker thread. */
        if (!edited &&
            !RawFormat::HasSensorDecoder(QFileInfo(fPath).suffix().toLower()))
            continue;
        paths << fPath;
    }
    if (paths.isEmpty()) return;
    buildDevPreviews(paths, "background");
}

void MW::clearDevPreviewCache()
{
/*
    Develop > Clear Developed Preview Cache. Previews are disposable -- every one can be
    re-rendered from the recipe in the image's sidecar -- but a folder of them is minutes
    of rendering, so confirm.

    Only the on-disk tier is cleared. The 256px thumbnails live in the sidecars and are
    the image's own property; deleting those would be an edit to the user's files, not a
    cache operation.
*/
    if (G::isLogger) G::log("MW::clearDevPreviewCache");

    const qint64 bytes = DevPreviewCache::instance().totalBytes();
    const int n = DevPreviewCache::instance().count();
    if (n == 0) {
        if (G::popup) G::popup->showPopup("The developed preview cache is empty.", 2000);
        return;
    }

    const QMessageBox::StandardButton ret = QMessageBox::question(
        this, "Clear developed preview cache",
        QString("Delete %1 cached developed previews (%2 GB)?\n\n"
                "Your develop edits are not affected -- they are stored with each image. "
                "Previews are rebuilt as you visit and edit images again.")
            .arg(n).arg(double(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    DevPreviewCache::instance().clear();

    /* The loupe may be showing pixels that came out of the cache. Nothing is stale --
       clearing does not change what an image looks like -- so there is no repaint to do
       beyond letting the next visit re-render. */
    if (G::popup)
        G::popup->showPopup(QString("Cleared %1 developed previews.").arg(n), 2000);
}
