#include "Main/mainwindow.h"
#include "FocusStack/fs.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>


void MW::focusStackFromSelection()
{
    QString srcFun = "MW::generateFocusStackFromSelection";
    if (G::isLogger || G::FSLog) G::log(srcFun, fsMethod);

    QStringList inputPaths;
    if (!dm->getSelectionOrPicks(inputPaths) || inputPaths.isEmpty()) {
        QString msg = "No images selected for focus stacking.";
        updateStatus(false, msg, srcFun);
        return;
    }

    generateFocusStack(inputPaths, fsMethod, /*isLocal*/true);
}

void MW::groupFocusStacks(QList<QStringList> &groups, const QStringList &paths)
{
/*
    Called by MW::generateFocusStack

    Rules:
    - Build time-contiguous groups (gap >= 2000ms starts a new group)
    - Only keep groups with 2+ items
    - If a group has only 1 item:
        * do NOT add it to groups
        * remove that item from the FocusStack input folder (delete file)
*/

    const QString srcFun = "MW::groupFocusStacks";
    if (G::isLogger || G::FSLog) G::log(srcFun);

    struct Item {
        QString   path;
        QDateTime created;
    };

    // --- Collect (path, created) and drop anything we can't resolve ----------
    QVector<Item> items;
    items.reserve(paths.size());

    for (const QString &path : paths)
    {
        QDateTime t;

        const int r = dm->rowFromPath(path);
        if (r >= 0) {
            t = dm->index(r, G::CreatedColumn).data().toDateTime();
        } else {
            /*
                External launch (e.g. from Lightroom): the files are not in the
                DataModel, so parse the created date directly. The bundled
                exiftool wrapper is unreliable here because the external app's
                launch environment lacks the PATH/perl it needs, so use the
                native C++ metadata parser instead.
            */
            QFileInfo fileInfo(path);
            metadata->loadImageMetadata(fileInfo, r, dm->instance,
                                        true, false, false, false, srcFun, true);
            t = metadata->m.createdDate;
        }

        if (!t.isValid()) {
            QString msg = "Skipping (invalid Created DateTimeOriginal):" + path;
            qDebug().noquote() << srcFun << msg;
            if (G::isLogger || G::FSLog) G::log(srcFun, msg);
            continue;
        }

        items.push_back({path, t});
    }

    if (items.isEmpty()) {
        QString msg = "No valid images (Created date missing).";
        updateStatus(false, msg, srcFun);
        if (G::isLogger || G::FSLog) G::log(srcFun, msg);
        return;
    }

    // --- Sort by creation time so stacks become contiguous -------------------
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        if (a.created == b.created) return a.path < b.path;
        return a.created < b.created;
    });

    // --- Group into stacks: new stack if gap >= 2000 ms ----------------------
    static constexpr qint64 groupGapMs = 2000;  // >= 2 seconds => new group

    auto finalizeCurrent = [&](QStringList &current)
    {
        if (current.size() >= 2) {
            groups.push_back(current);
        }
        current.clear();
    };

    QStringList current;
    current.reserve(items.size());

    QDateTime prevT;
    for (const Item &it : items)
    {
        if (!prevT.isValid()) {
            current << it.path;
            prevT = it.created;
            continue;
        }

        const qint64 gapMs = prevT.msecsTo(it.created);
        if (gapMs >= groupGapMs) {
            finalizeCurrent(current);
        }

        current << it.path;
        prevT = it.created;
    }

    // finalize last group
    finalizeCurrent(current);

    if (false) return;

    QString msg;
    for (int gi = 0; gi < groups.size(); ++gi) {
        msg = "FocusStack Group " + QString::number(gi + 1);
        if (G::FSLog) G::log(srcFun, msg);
        // qDebug().noquote() << "---- FocusStack Group" << (gi + 1) << "----";
        for (const QString &p : groups[gi]) {
            const int r = dm->rowFromPath(p);
            const QDateTime t = (r >= 0)
                                    ? dm->index(r, G::CreatedColumn).data().toDateTime()
                                    : QDateTime();

            const QString hhmmsszzz = t.isValid()
                                          ? t.time().toString("HH:mm:ss:zzz")
                                          : QString("??:??:??:???");

            // qDebug().noquote() << " " << p << " [" << hhmmsszzz << "]";
            msg = hhmmsszzz + " " + p;
        }
    }
}

void MW::generateFocusStack(const QStringList paths,
                            QString method,
                            const bool isLocal)
{
/*
    - Create worker thread with class instantiation fs.
    - Initiate fs
    - Finish fs code
    - Start worker thread

    Called locally from MW::focusStackFromSelection()
    Called remotely from MW::handleStartupArgs() from Lightroom

    Folder structure:

    remoteFolder            Source stack images (any file format), srcFolder
        inputFolder         "FocusStack" if remote, srcFolder if local
            grpFolder       Working folder for each group
                align       Aligned color pngs
                depth       Depth map and diagnostics
                fusion      Fused results when testing

    inputFolder contains the input focus stack images (tiff or png)
    srcFolder is where to save the fused result image
        - if local, srcFolder = inputFolder
        - if remote, srcFolder = inputFolder parent = Lightroom folder
*/
    QString srcFun = "MW::generateFocusStack";
    if (G::isLogger || G::FSLog) G::log(srcFun, "method = " + method);

    QString msg =
        "<p>Focus stacking initiated.</p>"
        "View progress on the bottom status progress bar.<br>"
        "Press \"ESC\" to abort focus stacking.<br>"
        "You can continue to use Winnow while the focus stack is processed."
        ;
    G::popup->showPopup(msg, 7000);

    /*
        No need to force the status progress bar visible: Progress shows its own
        container as soon as the FocusStack row is painted (updateProgress below),
        even when the folder is empty and MW::nullFiltration / MW::reset have
        cleared the other rows.
    */

    // --------------------------------------------------------------------
    // Create worker thread + FS fs object
    // --------------------------------------------------------------------
    fsThread = new QThread(this);
    // QThread *fsThread = new QThread(this);
    fsPipeline = new FS();
    fsPipeline->moveToThread(fsThread);
    // local req'd for lamda
    FS *fs = fsPipeline.data();

    // When the thread starts → run the FS pipeline
    connect(fsThread, &QThread::started, fs, [this, fs]() // Capture 'this' instead of 'fsThread'
            {
                fs->run();
                QMetaObject::invokeMethod(fsThread, "quit", Qt::QueuedConnection);
            });

    // Abort
    connect(this, &MW::abortFocusStack, fs, &FS::requestAbort, Qt::DirectConnection);

    // Status update
    connect(fs, &FS::updateStatus, this, &MW::updateStatus);

    // Progress update
    connect(fs, &FS::progress, this, [this](int current, int total) {
            progress->updateProgress(progressFocusStackRow, current, total, Qt::darkYellow);
    }, Qt::QueuedConnection);

    // Use Winnow to decode and return a cv::Mat
    connect(fs, &FS::requestImage, this, &MW::matFromQImage, Qt::BlockingQueuedConnection);

    // cleanup when finished
    connect(fsThread, &QThread::finished, fs, &QObject::deleteLater);
    connect(fsThread, &QThread::finished, fsThread,   &QObject::deleteLater);

    // --------------------------------------------------------------------
    // Initialize fs
    // --------------------------------------------------------------------

    method = "DMap";                    // Only "DMap" for now (set in pref)

    fs->o.method = method;
    fs->o.isLocal = isLocal;
    fs->o.enableOpenCL = true;
    // fs->o.removeTemp = fsRemoveTemp;    // for debugging (set in pref)

    // Group multiple stacks
    groupFocusStacks(fs->groups, paths);

    // No stacks found
    if (fs->groups.empty()) {
        QString msg = "Unable to find any focus stacks";
        if (G::isLogger || G::FSLog) G::log(srcFun, msg);
        G::popup->showPopup(msg);
        return;
    }

    /*
    Snapshot the metadata for every slice now, while we are on the GUI thread
    and the DataModel still describes these files. The worker passes each
    snapshot back with its decode request, so decoding never re-queries the
    live DataModel. This lets the user navigate to other folders while the
    stack processes without the decode failing (which previously crashed in
    MW::matFromQImage).
    */
    fs->metaSnapshot.clear();
    for (const QStringList &group : std::as_const(fs->groups)) {
        for (const QString &p : group) {
            if (fs->metaSnapshot.contains(p)) continue;
            QFileInfo fileInfo(p);
            int row = dm->proxyRowFromPath(p);
            metadata->loadImageMetadata(fileInfo, row, dm->instance,
                                        true, true, false, true, srcFun);
            fs->metaSnapshot.insert(p, metadata->m);
        }
    }

    // --------------------------------------------------------------------
    // Finished
    // --------------------------------------------------------------------

    connect(fs, &FS::finished, this, [=, this](bool success, bool aborted) {
        QString msg;

        // clear progress (Progress hides its container if no other row is live)
        progress->clearProgress(progressFocusStackRow);

        // aborted or failed
        if (!success) {
            if (aborted) {
                // User pressed ESC
                msg = "Focus stacking was aborted.";
                if (G::FSLog) G::log(srcFun, msg);
                updateStatus(false, msg);
                G::popup->showPopup(msg, 5000);
            } else {
                // Failure
                msg = "Focus stacking failed.";
                if (G::FSLog) G::log(srcFun, msg);
                updateStatus(false, msg);
                G::popup->showPopup(msg, 5000);
                G::issue("Error", msg, "MW::generateFocusStack");
            }
            return;
        }

        // Evaluate we have a result path
        if (G::fsFusedPaths.isEmpty()) {
            msg = "Focus stacking failed: No output path found.";
            if (G::FSLog) G::log(srcFun, msg);
            updateStatus(false, msg);
            G::issue("Error", msg, "MW::generateFocusStack");
            return;
        }

        // Update UI with the new file
        QString resultPath = G::fsFusedPaths.first();
        if (isLocal) {
            insertFiles(QStringList{resultPath});
        } else {
            folderAndFileSelectionChange(resultPath, "FS::threadFinished");
        }

        fsTree->updateCount();
        bookmarks->updateCount();

        // Handle Success
        QString nGroups = QVariant(G::fsFusedPaths.count()).toString();
        if (nGroups == "1") msg = nGroups + " focus stack completed";
        else msg = nGroups + " focus stacks completed";
        if (G::FSLog) G::log(srcFun, msg);
        G::popup->showPopup(msg, 3000);

    }, Qt::QueuedConnection);

    if (fsThread->isRunning()) return;


    // --------------------------------------------------------------------
    // Start
    // --------------------------------------------------------------------

    fsThread->start();
}

void MW::matFromQImage(QString fPath, ImageMetadata m, cv::Mat &mat)
{
    G::log("MW::matFromQImage", fPath);

    /*
    This slot runs on the GUI thread via a Qt::BlockingQueuedConnection from
    the focus-stack worker. Any exception thrown here unwinds through the Qt
    event loop and terminates the app (the worker's try/catch cannot catch a
    throw that happens on another thread). cv::cvtColor throws on an empty Mat,
    so every failure path must leave mat empty and return instead of throwing.
    FSLoader::load treats an empty Mat as a load failure and aborts the stack
    cleanly.

    The ImageMetadata 'm' is the snapshot captured up front in
    MW::generateFocusStack. Decoding from it (rather than re-querying the live
    DataModel) means the user can navigate to other folders while a stack is
    processing without the decode failing.
    */
    mat.release();

    if (m.fPath.isEmpty()) m.fPath = fPath;     // guard against a missing snapshot
    if (m.video) return;

    // decode the source image from the metadata snapshot (no DataModel lookup)
    ImageDecoder imageDecoder(0, dm, metadata);
    QImage src;
    if (!imageDecoder.decodeIndependent(src, metadata, m) || src.isNull()) {
        qWarning() << "MW::matFromQImage: decode failed, returning empty mat for" << fPath;
        return;
    }

    // Convert to Format_RGB888 for 3-channel or RGBA8888 for 4-channel
    // Avoid RGB32 as OpenCV expects 3 or 4 channels specifically
    QImage swapped = src.convertToFormat(QImage::Format_RGBA8888);
    if (swapped.isNull()) {
        qWarning() << "MW::matFromQImage: convertToFormat failed for" << fPath;
        return;
    }

    // Deep copy into the output Mat
    mat = cv::Mat(swapped.height(), swapped.width(), CV_8UC4,
                  (void*)swapped.bits(), swapped.bytesPerLine()).clone();

    // OpenCV expects BGRA, not RGBA. Swap the channels:
    cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGRA);
}

void MW::copyFocusStackWinnetPath()
{
    QString winnetPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    winnetPath += "/Winnets/FocusStack";
    QApplication::clipboard()->setText(winnetPath);
    QString msg = "Copied \"" + winnetPath + "\" to the clipboard";
    G::popup->showPopup(msg, 3000);
}
