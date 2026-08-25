#include "Main/mainwindow.h"
#include "Cache/devpreviewcache.h"

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

    ONE AT A TIME, ON THE EXISTING ENGINE. The work is done by MW::developPixelSource --
    the exporter's render path -- which already stages a full-resolution render correctly:
    the GUI thread owns the datamodel reads, the stored-recipe capture, the orientation
    and the mask prerequisites, while the decode and the composite run on
    developRenderPool. Reusing it means the builder cannot drift from what an export or a
    real develop render produces.

    Images are processed strictly sequentially -- the next one starts when the previous
    one's render returns. That is what keeps the builder inside the raw-decoder
    concurrency budget (see ImageCache::rawDecodeLimit and the memory-overrun history
    behind it): one in-flight scene-linear decode, whatever the folder size. It also
    means a long build never competes with the image the user is looking at.
*/

void MW::buildDevPreviews(const QStringList &paths, const QString &src)
{
/*
    Queue devPreviews for paths that carry a develop recipe but have no CURRENT preview.
    Re-running it after a partial build is free: everything already done is filtered out
    here, so the queue only ever holds real work.
*/
    if (G::isLogger) G::log("MW::buildDevPreviews", src);
    if (!developProperties || !dm) return;

    for (const QString &fPath : paths) {
        if (fPath.isEmpty()) continue;
        if (devPreviewBuildQueue.contains(fPath)) continue;
        if (fPath == devPreviewBuildCurrent) continue;
        if (!devPreviewNeedsBuild(fPath)) continue;
        devPreviewBuildQueue.append(fPath);
    }

    if (devPreviewBuildQueue.isEmpty()) {
        if (src == "menu")
            G::popup->showPopup("Every edited image already has a developed preview.",
                                2000);
        return;
    }

    devPreviewBuildTotal = devPreviewBuildQueue.count() + devPreviewBuildDone;
    if (G::popup) {
        G::popup->setProgressVisible(true);
        G::popup->setProgressMax(devPreviewBuildTotal);
        G::popup->setProgress(0);
        G::popup->showPopup("Building developed previews for " +
                            QString::number(devPreviewBuildTotal) + " images", 0, true, 1);
    }
    devPreviewBuildNext();
}

bool MW::devPreviewNeedsBuild(const QString &fPath) const
{
/*
    True when fPath has a develop recipe and the devPreview cache does not hold a preview
    matching it. Keyed on the RECIPE rather than on the stored preview key, so an image
    edited by another Winnow instance or on another machine is picked up too.

    Only the large tier is tested. The 256px thumbnail lives in the sidecar and is written
    in the same pass, so if one is missing both are.
*/
    if (fPath.isEmpty() || !developProperties) return false;
    const QString blob = developProperties->developBlobFor(fPath);
    if (blob.isEmpty()) return false;                   // no edits: nothing to depict
    return !DevPreviewCache::instance().contains(
        fPath, Metadata::devPreviewKey(blob).toLatin1());
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

    updateDevPreviewBuildProgress();

    /* 8-bit sRGB: a devPreview is a JPEG for the screen, not an export master. */
    developPixelSource(fPath, /*want16Bit*/false, OutputTransform::Space::sRGB,
        [this, fPath](bool ok, const QImage &out) {
            if (ok && !out.isNull()) devPreviewStore(fPath, out);
            ++devPreviewBuildDone;
            devPreviewBuildCurrent.clear();
            devPreviewBuildNext();
        });
}

void MW::devPreviewStore(const QString &fPath, const QImage &full)
{
/*
    Write both tiers for a freshly rendered full-resolution image: the 256px thumbnail
    into the image's XMP sidecar, and the devPreview into the on-disk cache.

    This deliberately mirrors the provider lambda in initialize.cpp rather than calling it
    -- the provider serves the image on screen and reads developFrame / developFullFrame,
    neither of which describes an image the builder just rendered off-thread.

    THE RECIPE IS RE-READ HERE, not captured when the image was queued. A render takes
    seconds and the user may have edited this image in between; keying the preview to the
    recipe in force at queue time would stamp it with a recipe it does not depict. If it
    has changed, the write is skipped and the image simply misses again.
*/
    if (G::isLogger) G::log("MW::devPreviewStore");
    if (!developProperties) return;

    const QString blob = developProperties->developBlobFor(fPath);
    if (blob.isEmpty()) return;                 // edits removed while this rendered
    const QString key = Metadata::devPreviewKey(blob);

    auto encode = [](const QImage &im, int quality, QByteArray &out) {
        QBuffer buf(&out);
        if (!buf.open(QIODevice::WriteOnly)) return false;
        return im.save(&buf, "JPG", quality);
    };

    QByteArray thumbJpg;
    const QImage thumb = full.scaled(G::maxIconSize, G::maxIconSize,
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!encode(thumb, 85, thumbJpg)) thumbJpg.clear();

    QByteArray previewJpg;
    QImage preview = full;
    const int cap = G::devPreviewMaxEdge;
    if (cap > 0 && qMax(preview.width(), preview.height()) > cap)
        preview = preview.scaled(cap, cap, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!encode(preview, 90, previewJpg)) previewJpg.clear();

    if (thumbJpg.isEmpty() && previewJpg.isEmpty()) return;

    /* Sidecar first: writeDevelopSidecar writes the recipe, the thumbnail and the key in
       ONE pass, so the three can never disagree. The recipe is passed through
       unchanged -- this is not a recipe edit. */
    if (!thumbJpg.isEmpty())
        Metadata::writeDevelopSidecar(fPath, blob, thumbJpg.toBase64());
    if (!previewJpg.isEmpty())
        DevPreviewCache::instance().put(fPath, key.toLatin1(), previewJpg);

    /* Bring the grid (and the image cache) into line, exactly as a flushed edit does. */
    devPreviewUpdated(fPath, thumbJpg.isEmpty() ? QImage() : thumb);
}

void MW::updateDevPreviewBuildProgress()
{
    if (!devPreviewBuildTotal || !G::popup) return;
    G::popup->setProgress(devPreviewBuildDone);
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

    if (!wasRunning) return;

    if (!G::popup) return;
    G::popup->setProgressVisible(false);
    G::popup->reset();
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
    Called once per folder load when the preference is on. Queues every edited image in
    the folder that has no current preview.
*/
    if (G::isLogger) G::log("MW::buildDevPreviewsInBackground");
    if (!G::buildDevPreviewsInBackground) return;
    if (!dm || dm->rowCount() == 0) return;

    QStringList paths;
    for (int row = 0; row < dm->rowCount(); ++row) {
        /* G::DevelopColumn is set from the sidecar during the metadata read, so this
           skips the unedited majority without opening a single file. */
        if (!dm->index(row, G::DevelopColumn).data().toBool()) continue;
        paths << dm->index(row, G::PathColumn).data(G::PathRole).toString();
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
