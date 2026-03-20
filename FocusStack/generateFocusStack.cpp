#include "Main/mainwindow.h"
#include "FocusStack/fs.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>


void MW::focusStackFromSelection()
{
    QString srcFun = "MW::generateFocusStackFromSelection";
    if (G::isLogger || G::FSLog) G::log(srcFun, fsMethod);

    QStringList inputPaths;
    if (!dm->getSelection(inputPaths) || inputPaths.isEmpty()) {
        QString msg = "No images selected for focus stacking.";
        updateStatus(false, msg, srcFun);
        return;
    }

    generateFocusStack(inputPaths, fsMethod, /*isLocal*/true);
}

void MW::groupFocusStacks(QList<QStringList> &groups, const QStringList &paths)
{
    // Called locally from MW::focusStackFromSelection
    // Called externally from MW::handleStartupArgs
    //
    // Rules:
    // - Build time-contiguous groups (gap >= 2000ms starts a new group)
    // - Only keep groups with 2+ items
    // - If a group has only 1 item:
    //     * do NOT add it to groups
    //     * remove that item from the FocusStack input folder (delete file)

    const QString srcFun = "MW::groupFocusStacks";
    if (G::isLogger) G::log(srcFun);

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
            ExifTool et;
            const QString creation = et.readTag(path, "DateTimeOriginal");
            et.close();
            t = QDateTime::fromString(creation, "yyyy:MM:dd HH:mm:ss");
        }

        if (!t.isValid()) {
            qDebug().noquote() << srcFun << "Skipping (invalid Created):" << path;
            continue;
        }

        items.push_back({path, t});
    }

    if (items.isEmpty()) {
        updateStatus(false, "No valid images (Created date missing).", srcFun);
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

    if(false) return;

    QString msg;
    for (int gi = 0; gi < groups.size(); ++gi) {
        msg = "FocusStack Group " + QString::number(gi + 1);
        if (G::isFileLogger) Utilities::log(srcFun, msg);
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
    // */
    // }
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

    G::popup->showPopup("Focus stacking initiated", 5000);

    // --------------------------------------------------------------------
    // Create worker thread + FS fs object
    // --------------------------------------------------------------------
    QThread *fsThread = new QThread(this);
    fsPipeline = new FS();
    fsPipeline->moveToThread(fsThread);
    // local req'd for lamda
    FS *fs = fsPipeline.data();

    // When the thread starts → run the FS pipeline
    connect(fsThread, &QThread::started, fs, [fs, fsThread]()
            {
                fs->run();     // runs synchronously inside worker thread
                QMetaObject::invokeMethod(fsThread, "quit", Qt::QueuedConnection);
            });

    // Abort
    connect(this, &MW::abortFocusStack, fs, &FS::requestAbort, Qt::DirectConnection);

    // Status update
    connect(fs, &FS::updateStatus, this, &MW::updateStatus);

    // Progress update
    connect(fs, &FS::progress, this, [this](int current, int total) {
        this->cacheProgressBar->updateUpperProgress(current, total, Qt::darkYellow);
    }, Qt::QueuedConnection);

    // cleanup when finished
    connect(fsThread, &QThread::finished, fs, &QObject::deleteLater);
    connect(fsThread, &QThread::finished, fsThread,   &QObject::deleteLater);

    // --------------------------------------------------------------------
    // Initialize fs
    // --------------------------------------------------------------------

    // Source images folder (used after pipeline finishes)
    QFileInfo info(paths.first());
    const QString inputFolderPath = info.absolutePath();
    QString srcFolderPath;
    // Source images location (ie when sourced from lightroom)
    if (isLocal) srcFolderPath = inputFolderPath;
    else {
        // get parent of inputFolder
        QFileInfo fi(inputFolderPath);
        srcFolderPath = fi.dir().absolutePath();
    }

    if (G::FSLog) G::log(srcFun, "srcFolder = " + srcFolderPath);

    if (!G::isRory) method = "DMap";
    if (!G::isRory) fsRemoveTemp = true;

    fs->o.method = method;
    fs->o.isLocal = isLocal;
    fs->o.enableOpenCL = true;
    fs->o.removeTemp = fsRemoveTemp;

    fs->srcFolderPath = srcFolderPath;

    // Group multiple stacks
    groupFocusStacks(fs->groups, paths);

    // --------------------------------------------------------------------
    // Finished
    // --------------------------------------------------------------------

    connect(fs, &FS::finished, this, [=](bool success) {
        QString msg;
        cacheProgressBar->clearUpperProgress();

        if (!success) {
            // Handle Abort or Failure
            msg = "Focus stacking was aborted or failed.";
            if (G::FSLog) G::log(srcFun, msg);
            updateStatus(false, msg);
            G::popup->showPopup(msg, 5000);
            return;
        }

        // Handle Success
        msg = "Focus stacking completed";
        if (G::FSLog) G::log(srcFun, msg);
        updateStatus(false, msg);
        G::popup->showPopup(msg, 5000);

        // Evaluate we have a result path
        if (G::fsFusedPaths.isEmpty()) {
            msg = "Focus stacking failed: No output path found.";
            if (G::FSLog) G::log(srcFun, msg);
            updateStatus(false, msg);
            return;
        }

        // Update UI with the new file
        QString resultPath = G::fsFusedPaths.first();
        if (isLocal) {
            dm->insert(resultPath);
            if (dm->contains(resultPath)) {
                sel->select(resultPath);
            }
        } else {
            folderAndFileSelectionChange(resultPath, "FS::threadFinished");
        }

        fsTree->updateCount();
        bookmarks->updateCount();
    }, Qt::QueuedConnection);

    if (fsThread->isRunning()) return;


    // --------------------------------------------------------------------
    // Start
    // --------------------------------------------------------------------

    fsThread->start();
}

