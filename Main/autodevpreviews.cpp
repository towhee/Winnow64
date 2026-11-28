#include "Main/mainwindow.h"
#include "Main/devpreviewpolicy.h"
#include "Cache/devpreviewcache.h"
#include "Cache/imagedecoder.h"
#include "ImageFormats/Raw/rawformat.h"
#include "Utilities/versionkey.h"
#include "Utilities/backgroundpriority.h"
#include <QStorageInfo>
#include <QMessageBox>
#include <QPushButton>
#if defined(Q_OS_MAC)
#include "Utilities/mac.h"
#endif

/*
    autoDevPreviews: devPreviews for the thumbnails ON SCREEN, built while the user
    pauses.

    WHAT IT BUILDS. The thumbnails the user can see (MW::visibleThumbnailKeys), nearest
    the current image first (DevPreviewPolicy::nearestFirst), minus the current image
    itself, whose picture is on screen and must not change under the user. Of those,
    only what DevPreviewPolicy says is worth it:

      edited             always -- without a devPreview Browse shows the image unedited
      small-preview raw  the camera's embedded JPEG is too small for the loupe (a Sony
                         ARW carries 1616 px), so Browse shows it soft and upscaled
      slow non-raw       NOT here: a 16-bit TIFF is HARVESTED from the loupe's own decode
                         (harvestDevPreview), which costs no render at all

    A raw whose embedded preview is good enough is NOT built: Browse already shows it at
    JPEG speed, and a default render would change the look and spend the cache.

    WHEN. Only after the user has left the selection and the scroll alone for
    DevPreviewPolicy::kAutoDelayMs (autoDevPreviewNudge restarts the wait on every
    selection change and scroll, so riffling never starts anything), and only while the
    gate holds (autoDevPreviewGate): Browse mode, nothing loading, the ImageCache's
    read-ahead finished (it is serving the user), no memory or thermal pressure, and room
    in the cache. The gate is checked again before EVERY image, so each image boundary is
    an interruption point.

    HOW. It rides the existing builder (Main/devpreviewbuilder.cpp) as src "auto": queued
    ahead of a background run but behind anything the user asked for, one render at a
    time on developPixelSource, at background priority (Utilities/backgroundpriority.h).
    It is quiet: no progress row (a row blinking on and off as the user pauses and moves
    would be noise about work they never asked to watch); the green thumbnail badge
    shows each preview as it lands.

    INTERRUPTION. A selection change or a scroll drops every queued auto image at once
    and CANCELS the render in flight: developPixelSource checks the token before the
    decode, aborts the demosaic at its next tile, and checks again before the masks and
    the composite. The composite itself cannot be stopped; it is the shorter half.

    PERFORMANCE. While a step runs, autoDevPreviewWatchTick checks every
    kAutoWatchMs: an event-loop stall past DevPreviewPolicy::kAutoLagMs, the read-ahead
    busy for half a second, memory pressure or (macOS) a serious thermal state cancels
    the step, and the next pass waits longer (autoDevPreviewBackoffMs, doubling to a
    minute; a step that completes resets it). An image cancelled this way
    kAutoPerfMissLimit times is skipped for the session.

    With G::isPerfProbe every start, finish and cancel prints one [AutoDevPreview] line,
    so the thresholds above can be checked against what the machine actually does.
*/

namespace {
constexpr int kAutoWatchMs = 100;            // the performance watch's tick
constexpr int kAutoBusyTicks = 5;            // read-ahead busy this many ticks = 0.5 s
constexpr int kAutoBackoffStartMs = 5000;
constexpr int kAutoBackoffMaxMs = 60000;
constexpr int kHarvestMaxPending = 2;

void autoLog(const QString &msg)
{
    if (G::isLogger) G::log("autoDevPreviews", msg);
    if (G::isPerfProbe) qDebug().noquote() << "[AutoDevPreview]" << msg;
}

/* Thermal pressure: serious or worse means the machine is already throttling. macOS
   reports it; Windows and Linux have no portable signal, so they never pause for it. */
bool thermallyThrottled()
{
#if defined(Q_OS_MAC)
    return Mac::thermalState() >= 2;
#elif defined(Q_OS_WIN)
    return false;
#elif defined(Q_OS_LINUX)
    return false;
#else
    return false;
#endif
}
}  // namespace

void MW::initAutoDevPreviews()
{
/*
    Called once at startup, before any folder loads, so the harvest hook is in place
    before the first decoder runs and never changes after.
*/
    if (G::isLogger) G::log("MW::initAutoDevPreviews");

    autoDevPreviewTimer = new QTimer(this);
    autoDevPreviewTimer->setSingleShot(true);
    connect(autoDevPreviewTimer, &QTimer::timeout, this, &MW::autoDevPreviewTick);

    autoDevPreviewWatch = new QTimer(this);
    autoDevPreviewWatch->setInterval(kAutoWatchMs);
    connect(autoDevPreviewWatch, &QTimer::timeout, this, &MW::autoDevPreviewWatchTick);

    /* One thread: harvests are a color conversion and a JPEG encode, never urgent, and
       one at a time keeps at most kHarvestMaxPending full-size images alive. */
    devPreviewHarvestPool = new QThreadPool(this);
    devPreviewHarvestPool->setMaxThreadCount(1);
    devPreviewHarvestPool->setThreadPriority(QThread::LowPriority);

    /* DECODER THREAD. Reserve a slot before queueing the pixels to the GUI thread: a
       riffle through a TIFF folder would otherwise queue a full-size image per decode
       faster than they could be encoded. A refused offer is not lost -- the next decode
       of that image offers again. */
    ImageDecoder::harvestHook = [this](const QString &rowKey, const QImage &image,
                                       const QByteArray &icc, qint64 ms) {
        int n = devPreviewHarvestPending.load();
        do {
            if (n >= kHarvestMaxPending) return;
        } while (!devPreviewHarvestPending.compare_exchange_weak(n, n + 1));
        QMetaObject::invokeMethod(this, [this, rowKey, image, icc, ms] {
            harvestDevPreview(rowKey, image, icc, ms);
        }, Qt::QueuedConnection);
    };
}

void MW::autoDevPreviewNudge(const QString &why)
{
/*
    The user did something. Drop what was queued for the old position, cancel the render
    in flight, and start the wait again. Called on every selection change and scroll
    event, so it must stay cheap: a timer restart, plus work only when a pass is running.
*/
    if (!autoDevPreviewTimer) return;
    if (!G::autoDevPreviews) return;
    interruptAutoDevPreviews(why);
    autoDevPreviewTimer->start(qMax(DevPreviewPolicy::kAutoDelayMs, autoDevPreviewBackoffMs));
}

void MW::interruptAutoDevPreviews(const QString &why)
{
    if (autoDevPreviewTimer) autoDevPreviewTimer->stop();

    if (!autoDevPreviewPaths.isEmpty()) {
        int n = 0;
        for (const QString &p : std::as_const(autoDevPreviewPaths))
            n += devPreviewBuildQueue.removeAll(p);
        autoDevPreviewPaths.clear();
        if (n > 0) {
            devPreviewBuildTotal = qMax(devPreviewBuildDone, devPreviewBuildTotal - n);
            autoLog(QString("dropped %1 queued (%2)").arg(n).arg(why));
            if (devPreviewBuildQueue.isEmpty() && devPreviewBuildCurrent.isEmpty()
                && devPreviewBuildTotal > 0)
                devPreviewBuildFinish();
            else updateDevPreviewBuildProgress();
        }
    }

    /* An input cancel, not a performance one: the reason stays empty so the image is
       not counted against (autoDevPreviewStepEnded). */
    if (!autoDevPreviewCurrent.isEmpty() && autoDevPreviewCancel
        && !autoDevPreviewCancel->load()) {
        autoDevPreviewCancelWhy.clear();
        autoDevPreviewCancel->store(true);
        autoLog(QString("cancel %1 (%2)").arg(autoDevPreviewCurrent, why));
    }
}

bool MW::autoDevPreviewGate(QString *why, bool *retry)
{
    auto no = [&](const QString &w, bool r) {
        if (why) *why = w;
        if (retry) *retry = r;
        return false;
    };
    if (!G::autoDevPreviews) return no("off", false);
    if (!dm || !developProperties || !imageCache) return no("not ready", false);
    /* Develop renders the image being edited on the builder's one thread; leaving
       Develop nudges (setOperationMode). */
    if (G::operationMode == G::OperationMode::Develop) return no("Develop mode", false);
    if (G::isSlideShow) return no("slide show", false);     // its own look-ahead builds
    if (G::stop || G::isLoadRunning.load() || G::isModifyingDatamodel.load())
        return no("loading", true);
    /* The read-ahead is serving the user's next images: it goes first, always. */
    if (!imageCache->isIdle()) return no("read-ahead busy", true);
    if (imageCache->isMemoryThrottled()) return no("memory pressure", true);
    if (thermallyThrottled()) return no("thermal pressure", true);
    if (adviseDevPreviewCacheFull()) return no("cache nearly full", false);
    return true;
}

bool MW::devPreviewWanted(const QString &fPath) const
{
/*
    Is this row worth a SPECULATIVE build (autoDevPreviews, the background build)? Edited
    rows always are; an unedited raw only when its embedded preview is too small for the
    loupe (DevPreviewPolicy::smallPreviewRaw). Slow non-raws are the harvest's, never a
    render's. GUI thread: model reads, no file I/O.
*/
    if (!dm) return false;
    const int row = dm->rowFromKey(fPath);
    if (row < 0) return false;
    if (dm->index(row, G::DevelopColumn).data().toBool()) return true;
    const QString ext = QFileInfo(VersionKey::sourceOf(fPath)).suffix().toLower();
    if (!RawFormat::HasSensorDecoder(ext)) return false;
    return DevPreviewPolicy::smallPreviewRaw(
        dm->index(row, G::WidthOrigPreviewColumn).data().toInt(),
        dm->index(row, G::HeightOrigPreviewColumn).data().toInt(),
        dm->index(row, G::WidthColumn).data().toInt(),
        dm->index(row, G::HeightColumn).data().toInt(),
        devPreviewScreenEdge());
}

int MW::devPreviewScreenEdge() const
{
/*
    The long edge, in device pixels, of the screen the main window is on: what a loupe
    at fit can actually show. A preview at least this big (or as big as the sensor)
    looks as good in Browse as a render would.
*/
    QScreen *s = windowHandle() ? windowHandle()->screen() : nullptr;
    if (!s) s = QGuiApplication::primaryScreen();
    if (!s) return 0;
    const QSize g = s->geometry().size();
    return qRound(qMax(g.width(), g.height()) * s->devicePixelRatio());
}

void MW::autoDevPreviewTick()
{
/*
    The pause is over. If the gate holds, queue the visible thumbnails that want a
    devPreview, nearest the current image first; the builder takes it from there and
    re-checks the gate before each image. If it does not hold and waiting could help
    (the read-ahead is still busy), look again after another pause.
*/
    QString why;
    bool retry = false;
    if (!autoDevPreviewGate(&why, &retry)) {
        if (retry && autoDevPreviewTimer)
            autoDevPreviewTimer->start(qMax(DevPreviewPolicy::kAutoDelayMs,
                                            autoDevPreviewBackoffMs));
        if (G::isPerfProbe) autoLog("waiting: " + why);
        return;
    }
    /* Still queued: a pass from this same position (a nudge would have cleared it). */
    if (!autoDevPreviewPaths.isEmpty()) return;
    /* A render still in flight, usually one just cancelled and finishing its composite
       (which cannot be stopped): look again shortly rather than queue beside it. */
    if (!autoDevPreviewCurrent.isEmpty()) {
        if (autoDevPreviewTimer) autoDevPreviewTimer->start(DevPreviewPolicy::kAutoDelayMs);
        return;
    }

    /* Visible rows, nearest first. visibleThumbnailKeys returns proxy order. */
    const QStringList keys = visibleThumbnailKeys();
    QList<int> rows;
    QHash<int, QString> keyAt;
    for (const QString &k : keys) {
        const int dmRow = dm->rowFromKey(k);
        if (dmRow < 0) continue;
        const int sfRow = dm->sf->mapFromSource(dm->index(dmRow, 0)).row();
        if (sfRow < 0) continue;
        rows << sfRow;
        keyAt.insert(sfRow, k);
    }
    const QList<int> ordered = DevPreviewPolicy::nearestFirst(rows, dm->currentSfRow);

    QHash<QString, bool> eligible;          // per folder, this pass only
    QStringList paths;
    for (int r : ordered) {
        const QString fPath = keyAt.value(r);
        if (fPath.isEmpty() || fPath == dm->currentKey) continue;
        if (autoDevPreviewPerfMisses.value(fPath) >= DevPreviewPolicy::kAutoPerfMissLimit)
            continue;
        const QString folder = QFileInfo(VersionKey::sourceOf(fPath)).absoluteDir().path();
        auto it = eligible.find(folder);
        if (it == eligible.end())
            it = eligible.insert(folder,
                                 DevPreviewPolicy::folderEligible(catalogScope, folder));
        if (!it.value()) continue;
        if (!devPreviewWanted(fPath)) continue;
        paths << fPath;
    }
    if (paths.isEmpty()) {
        if (G::isPerfProbe) autoLog("nothing on screen wants a preview");
        return;
    }
    autoLog(QString("pass: %1 candidates of %2 visible").arg(paths.count())
                .arg(keys.count()));
    /* buildDevPreviews derives each key (and drops what has none), filters what the
       cache already holds off the GUI thread, then queues the rest as src "auto". */
    buildDevPreviews(paths, "auto");
}

void MW::autoDevPreviewWatchTick()
{
/*
    The performance watch, every kAutoWatchMs while an auto render is in flight. Any
    sign that the step is costing the user cancels it; autoDevPreviewStepEnded then
    backs the next pass off and counts the miss against the image.
*/
    if (autoDevPreviewCurrent.isEmpty() || !autoDevPreviewCancel
        || autoDevPreviewCancel->load()) {
        autoDevPreviewWatch->stop();
        return;
    }
    const qint64 gap = autoDevPreviewWatchClock.restart();
    QString why;
    if (gap > kAutoWatchMs + DevPreviewPolicy::kAutoLagMs)
        why = QString("event loop stalled %1 ms").arg(gap);
    else if (imageCache && !imageCache->isIdle()) {
        /* Sustained, not a blip: the build's own write re-decodes its image from the
           new preview, which keeps a decoder busy for a moment. */
        if (++autoDevPreviewBusyTicks >= kAutoBusyTicks) why = "read-ahead busy";
    }
    else autoDevPreviewBusyTicks = 0;
    if (why.isEmpty() && imageCache && imageCache->isMemoryThrottled())
        why = "memory pressure";
    if (why.isEmpty() && thermallyThrottled()) why = "thermal pressure";
    if (why.isEmpty()) return;

    autoDevPreviewWatch->stop();
    autoDevPreviewCancelWhy = why;
    autoDevPreviewCancel->store(true);
    autoLog(QString("cancel %1 (%2)").arg(autoDevPreviewCurrent, why));
    /* Nothing else queued for this pass starts either. */
    if (!autoDevPreviewPaths.isEmpty()) {
        int n = 0;
        for (const QString &p : std::as_const(autoDevPreviewPaths))
            n += devPreviewBuildQueue.removeAll(p);
        autoDevPreviewPaths.clear();
        devPreviewBuildTotal = qMax(devPreviewBuildDone, devPreviewBuildTotal - n);
    }
}

void MW::autoDevPreviewStepEnded(const QString &fPath, bool cancelled)
{
/*
    An auto render is over: built, failed, or cancelled. A PERFORMANCE cancel
    (autoDevPreviewCancelWhy set) counts against the image and backs the next pass off;
    an input cancel does neither -- the user moving on says nothing about the image.
*/
    // a stale callback must not end the step that replaced it
    if (fPath != autoDevPreviewCurrent) return;
    autoDevPreviewCurrent.clear();
    autoDevPreviewCancel.reset();
    autoDevPreviewBusyTicks = 0;
    if (autoDevPreviewWatch) autoDevPreviewWatch->stop();
    const qint64 ms = autoDevPreviewStepClock.isValid()
                          ? autoDevPreviewStepClock.elapsed() : 0;
    const QString name = QFileInfo(fPath).fileName();

    if (cancelled && !autoDevPreviewCancelWhy.isEmpty()) {
        const int misses = ++autoDevPreviewPerfMisses[fPath];
        autoDevPreviewBackoffMs = autoDevPreviewBackoffMs
                                      ? qMin(kAutoBackoffMaxMs, autoDevPreviewBackoffMs * 2)
                                      : kAutoBackoffStartMs;
        autoLog(QString("%1 cancelled after %2 ms: %3 (miss %4, next pass in %5 s)")
                    .arg(name).arg(ms).arg(autoDevPreviewCancelWhy).arg(misses)
                    .arg(autoDevPreviewBackoffMs / 1000));
        if (G::autoDevPreviews && autoDevPreviewTimer)
            autoDevPreviewTimer->start(autoDevPreviewBackoffMs);
    }
    else if (cancelled) {
        autoLog(QString("%1 cancelled after %2 ms by input").arg(name).arg(ms));
    }
    else {
        autoDevPreviewBackoffMs = 0;
        autoLog(QString("%1 built in %2 ms").arg(name).arg(ms));
    }
    autoDevPreviewCancelWhy.clear();
}

void MW::harvestDevPreview(const QString &rowKey, const QImage &image,
                           const QByteArray &icc, qint64 ms)
{
/*
    Keep a slow non-raw decode (ImageDecoder::offerHarvest) as its devPreview, so the
    next visit decodes a JPEG instead of the file. GUI thread for the decision -- the
    folder's catalog scope and the cache's fill are GUI-thread state -- then a
    background thread for the work.

    DISPLAY P3, NOT sRGB. The harvested pixels are the file's own, and a wide-gamut TIFF
    (ProPhoto, Adobe RGB) has colors sRGB cannot hold; P3 keeps what a P3 display can
    show. The profile is embedded in the JPEG and ImageDecoder::colorManage converts from
    it, so the loupe shows the same picture it showed when it decoded the file.

    STAMPED BEFORE, CHECKED AFTER. The file's size and mtime are taken here and compared
    again just before the put: a file rewritten while its harvest was encoding would
    otherwise be stamped as the new file with the old pixels.

    devPreviewHarvestPending was reserved by the hook; every path out releases it.
*/
    auto release = [this, rowKey] {
        devPreviewHarvestPaths.remove(rowKey);
        devPreviewHarvestPending.fetch_sub(1);
    };
    if (!G::autoDevPreviews || !dm || image.isNull() || dm->rowFromKey(rowKey) < 0
        || devPreviewHarvestPaths.contains(rowKey)) {
        devPreviewHarvestPending.fetch_sub(1);
        return;
    }
    const QString src = VersionKey::sourceOf(rowKey);
    const QString folder = QFileInfo(src).absoluteDir().path();
    if (!DevPreviewPolicy::folderEligible(catalogScope, folder)
        || adviseDevPreviewCacheFull()) {
        devPreviewHarvestPending.fetch_sub(1);
        return;
    }
    devPreviewHarvestPaths.insert(rowKey);

    const QString key = Metadata::sourceRenderKey(rowKey);
    const QPair<qint64, qint64> stamp = DevPreviewCache::fileStamp(src);
    const int cap = G::devPreviewMaxEdge;
    const int quality = G::devPreviewQuality;
    autoLog(QString("harvest %1 (decode %2 ms)").arg(QFileInfo(src).fileName()).arg(ms));

    devPreviewHarvestPool->start([this, rowKey, src, key, stamp, image, icc, cap, quality,
                                  release] {
        QElapsedTimer t;
        t.start();
        bool stored = false;
        if (!DevPreviewCache::instance().contains(rowKey, key.toLatin1())) {
            QImage im = image;
            /* convertToColorSpace needs RGB pixels: grayscale and indexed decodes are
               widened first, 16-bit ones kept 16-bit until the encode. */
            im = im.convertToFormat(im.depth() > 32 ? QImage::Format_RGBA64
                                                    : QImage::Format_RGB32);
            QColorSpace from = icc.isEmpty() ? QColorSpace(QColorSpace::SRgb)
                                             : QColorSpace::fromIccProfile(icc);
            if (!from.isValid()) from = QColorSpace(QColorSpace::SRgb);
            im.setColorSpace(from);
            im.convertToColorSpace(QColorSpace(QColorSpace::DisplayP3));
            if (cap > 0 && qMax(im.width(), im.height()) > cap)
                im = im.scaled(cap, cap, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (im.depth() > 32) im = im.convertToFormat(QImage::Format_RGB32);
            QByteArray jpg;
            QBuffer buf(&jpg);
            const bool ok = buf.open(QIODevice::WriteOnly) && im.save(&buf, "JPG", quality);
            if (ok && !jpg.isEmpty() && DevPreviewCache::fileStamp(src) == stamp) {
                DevPreviewCache::instance().put(rowKey, key.toLatin1(), jpg);
                stored = true;
            }
        }
        const qint64 ms = t.elapsed();
        QMetaObject::invokeMethod(this, [rowKey, stored, ms, release] {
            release();
            if (stored)
                autoLog(QString("harvested %1 in %2 ms")
                            .arg(QFileInfo(rowKey).fileName()).arg(ms));
        }, Qt::QueuedConnection);
    });
}

bool MW::adviseDevPreviewCacheFull()
{
/*
    Is the cache too full for speculative work? DevPreviewPolicy::nearlyFull. From there
    on, autoDevPreviews, the background build and the harvest stop rather than evict:
    they would only be trading previews the user may want for previews nobody asked for.
    Editing (byproduct writes) and Develop > Build Developed Previews keep the LRU.

    The first time in a session it is true, the user is told, once, with the numbers to
    decide on: the limit, what is used, the free space on the cache's disk, and a
    suggested larger limit (DevPreviewPolicy::suggestCap) when the disk can afford one.
    The dialog is POSTED, not run here: the callers are inside the builder and the
    gate, and a nested event loop there could re-enter them.
*/
    DevPreviewCache &c = DevPreviewCache::instance();
    const qint64 used = c.totalBytes();
    const qint64 cap = c.maxBytes();
    if (!DevPreviewPolicy::nearlyFull(used, cap)) return false;
    if (devPreviewCacheAdvised) return true;
    devPreviewCacheAdvised = true;
    autoLog(QString("cache nearly full: %1 of %2 bytes").arg(used).arg(cap));

    QTimer::singleShot(0, this, [this] {
        DevPreviewCache &c = DevPreviewCache::instance();
        const qint64 used = c.totalBytes();
        const qint64 cap = c.maxBytes();
        const QStorageInfo disk(c.cacheDir());
        const qint64 freeBytes = disk.isValid() ? disk.bytesAvailable() : 0;
        constexpr qint64 GB = 1024LL * 1024 * 1024;
        QList<qint64> steps;
        for (qint64 s : {5, 10, 20, 50, 100, 200}) steps << s * GB;
        const qint64 suggest = DevPreviewPolicy::suggestCap(used, cap, freeBytes, steps);
        auto gb = [](qint64 b) { return QString::number(double(b) / GB, 'f', 1); };
        const QString volume = disk.isValid() && !disk.displayName().isEmpty()
                                   ? disk.displayName() : QString("the disk");

        QString text =
            QString("The developed preview cache is nearly full: %1 GB of its %2 GB "
                    "limit is used. %3 has %4 GB free.")
                .arg(gb(used), gb(cap), volume, gb(freeBytes));
        QString info =
            "Previews for the thumbnails on screen, and the background build, are "
            "paused so they do not remove previews you already have. Previews made "
            "while you edit are still saved. Your edits are never affected.";
        if (suggest > 0)
            info += QString("\n\nRaising the limit to %1 GB lets them continue.")
                        .arg(suggest / GB);
        else if (freeBytes < 10 * GB)
            info += "\n\nThe disk itself is short of space, so the limit is best "
                    "left as it is.";

        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle("Developed preview cache");
        box.setText(text);
        box.setInformativeText(info);
        QPushButton *increase = nullptr;
        if (suggest > 0)
            increase = box.addButton(QString("Increase to %1 GB").arg(suggest / GB),
                                     QMessageBox::AcceptRole);
        QPushButton *settings = box.addButton("Settings...", QMessageBox::ActionRole);
        QPushButton *notNow = box.addButton("Not now", QMessageBox::RejectRole);
        box.setDefaultButton(increase ? increase : notNow);
        box.exec();

        if (increase && box.clickedButton() == increase) {
            G::devPreviewCacheMaxBytes = suggest;
            this->settings->setValue("devPreviewCacheMaxBytes", suggest);
            c.setMaxBytes(suggest);
            /* suggest is one of the offered sizes, so its label is the combo's own
               ("50 GB", Preferences::devPreviewCacheLabel). */
            if (preferencesDlg && pref)
                pref->setItemValue("devPreviewCacheSize",
                                   QString("%1 GB").arg(suggest / GB));
            /* Advise again should the new limit fill up too. */
            devPreviewCacheAdvised = false;
            autoLog(QString("cache limit raised to %1 GB").arg(suggest / GB));
            autoDevPreviewNudge("cache limit raised");
        }
        else if (box.clickedButton() == settings) {
            cachePreferences();
        }
    });
    return true;
}
