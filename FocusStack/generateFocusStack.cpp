#include "Main/mainwindow.h"
#include "FocusStack/fs.h"


/*
Winnow64/FocusStack/

*/

void MW::focusStackFromSelection()
{
    QString src = "MW::generateFocusStackFromSelection";
    if (G::isLogger || G::FSLog) G::log(src, fsMethod);

    QStringList paths;
    if (!dm->getSelection(paths) || paths.isEmpty()) {
        QString msg = "No images selected for focus stacking.";
        updateStatus(false, msg, src);
        return;
    }

    // fsMethod is from settings

    generateFocusStack(paths, fsMethod, src);
}

void MW::groupFocusStacks(QList<QStringList> &groups, const QStringList &paths)
{
/*
    Called locally from MW::focusStackFromSelection
    Called externally from MW::handleStartupArgs

    Aggregates paths into focus stack groups, as there might be multiple
    focus stacks in the paths list.

    Generates a fused image for each stack group.
*/
    const QString srcFun = "MW::groupFocusStacks";
    if (G::isLogger) G::log(srcFun);

    struct Item {
        QString   path;
        QDateTime created;
    };

    // --- Collect (path, created) and drop anything we can't resolve ----------
    QVector<Item> items;
    items.reserve(paths.size());

    QDateTime t;
    for (const QString &path : paths) {
        const int r = dm->rowFromPath(path);
        // already in datamodel
        if (r >= 0) {
            t = dm->index(r, G::CreatedColumn).data().toDateTime();
        }
        // not in datamodel, probably remote call (lightroom)
        else {
            ExifTool et;
            QString creation = et.readTag(path, "DateTimeOriginal");
            et.close();
            t = QDateTime::fromString(creation, "yyyy:MM:dd HH:mm:ss");

            qDebug() << srcFun
                     << creation
                     << t
                     << path;
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
    static constexpr qint64 kGroupGapMs = 2000;  // >= 2 seconds => new group

    // QList<QStringList> groups;
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
        if (gapMs >= kGroupGapMs) {
            if (!current.isEmpty())
                groups.push_back(current);
            current.clear();
        }

        current << it.path;
        prevT = it.created;
    }
    if (!current.isEmpty()) groups.push_back(current);


    //  --- Debug dump with HH:mm:ss:zzz ---------------------------------------
    /*
    qDebug().noquote() << srcFun
                       << "Grouped" << items.size() << "images into"
                       << groups.size() << "stack(s)."
                       << "gapThresholdMs=" << kGroupGapMs;

    for (int gi = 0; gi < groups.size(); ++gi) {
        qDebug().noquote() << "---- FocusStack Group" << (gi + 1) << "----";
        for (const QString &p : groups[gi]) {
            const int r = dm->rowFromPath(p);
            const QDateTime t = (r >= 0)
                                    ? dm->index(r, G::CreatedColumn).data().toDateTime()
                                    : QDateTime();

            const QString hhmmsszzz = t.isValid()
                                          ? t.time().toString("HH:mm:ss:zzz")
                                          : QString("??:??:??:???");

            qDebug().noquote() << " " << p << " [" << hhmmsszzz << "]";
        }
    }
    //*/
}

void MW::generateFocusStack(const QStringList paths,
                            QString method,
                            const QString source)
{
/*
    Called locally from MW::focusStackFromSelection()
    Called remotely from MW::handleStartupArgs() from Lightroom

    Folders:
        Lightroom folder (only if remote)
            srcFolder (focus stack input tiffs)
                grpFolder ie 2025-11-07_0078_DMap
                    alignFolder
                    depthFolder
                    fusionFolder

    srcFolder contains the input focus stack images
    dstFolder is where to save the fused result image
        - if local, use srcFolder
        - if remote, use srcFolder parent = Lightroom folder
*/
    QString srcFun = "MW::generateFocusStack";
    if (G::isLogger || G::FSLog) G::log(srcFun, "method = " + method);

    G::isRunningFocusStack = true;
    G::abortFocusStack = false;

    bool success = true;

    bool isLocal = (source == "MW::generateFocusStackFromSelection");
    // isLocal = false;    // temp for debugging

    // ----------- Req'd when pipeline is finished -----------

    // Source images folder (used after pipeline finishes)
    QFileInfo info(paths.first());
    const QString srcFolderPath = info.absolutePath();
    // Source images location (ie when sourced from lightroom)
    QString dstFolderPath;
    if (isLocal) dstFolderPath = srcFolderPath;
    else {
        // get parent of srcFolder
        QFileInfo fi(srcFolderPath);
        dstFolderPath = fi.dir().absolutePath();
    }

    // fusedPath: must be the same as in FS::saveFused
    QFileInfo lastFi(paths.last());
    QString fusedBase = lastFi.completeBaseName() + "_FocusStack_" + method;
    QString ext  = "." + lastFi.suffix();
    QString dstLastFusedPath = dstFolderPath + "/" + fusedBase + ext;
    // Make sure unique file name
    Utilities::uniqueFilePath(dstLastFusedPath, "_");
    QFileInfo fusedFi(dstLastFusedPath);
    fusedBase = fusedFi.completeBaseName();

    if (G::FSLog) G::log(srcFun, "srcFolder =v" + srcFolderPath);
    if (G::FSLog) G::log(srcFun, "dstLastFusedPath =v" + dstLastFusedPath);

    // ----------- End section when pipeline is finished -----------

    // Options
    // clean (send all project folders to the trash)
    bool isClean = false;       // send all project folders to the trash


    // FOCUS STACK CONTROL:
    FS::Options opt;
    {
        opt.method                  = method;
        opt.isLocal                 = isLocal;    // pretend remote for testing
        opt.enableOpenCL            = true;
    }

    // --------------------------------------------------------------------
    // Create worker thread + FS pipeline object
    // --------------------------------------------------------------------
    QThread *fsThread = new QThread(this);
    fsPipeline = new FS();
    fsPipeline->moveToThread(fsThread);
    // local req'd for lamda
    FS *pipeline = fsPipeline.data();

    // Initialize pipeline before we run
    pipeline->setOptions(opt);
    pipeline->initialize(dstFolderPath, fusedBase);
    // Aggregate input paths into focus groups
    groupFocusStacks(pipeline->groups, paths);

    // -------------- Pipeline communications ----------------

    // When the thread starts → run the FS pipeline
    connect(fsThread, &QThread::started, pipeline, [pipeline, fsThread]()
    {
        pipeline->run();     // runs synchronously inside worker thread
        QMetaObject::invokeMethod(fsThread, "quit", Qt::QueuedConnection);
    });

    // Status update
    connect(pipeline, &FS::updateStatus,
            this, &MW::updateStatus, Qt::QueuedConnection);

    // Progress update
    connect(pipeline, &FS::progress, this, [this](int current, int total) {
        this->cacheProgressBar->updateUpperProgress(current, total, Qt::darkYellow);
        }, Qt::QueuedConnection);

    // cleanup when finished
    connect(fsThread, &QThread::finished, pipeline, &QObject::deleteLater);
    connect(fsThread, &QThread::finished, fsThread,   &QObject::deleteLater);

    // --------------- When FS finishes (thread quits) ----------------

    // We detect success by checking filesystem or pipeline signals (later)
    connect(fsThread, &QThread::finished, this, [=]()
    {
        QString msg = "FS is finished.";
        if (G::FSLog) G::log(srcFun, msg);

        G::isRunningFocusStack = false;

        // Clear progress
        cacheProgressBar->clearUpperProgress();

        // If aborted...
        if (G::abortFocusStack) {
            msg = "Focus stacking was aborted.";
            if (G::FSLog) G::log(srcFun, msg);
            G::abortFocusStack = false;
            updateStatus(false, msg);
            G::popup->showPopup(msg);
            return;
        }

        msg = "Focus stacking completed";
        if (G::FSLog) G::log(srcFun, msg);
        updateStatus(false, msg);
        G::popup->showPopup(msg);


        // Evaluate we have a result path
        if (dstLastFusedPath.isEmpty() || QImage(dstLastFusedPath).isNull()) {
            msg = "Focus stacking failed";
            if (G::FSLog) G::log(srcFun, msg);
            updateStatus(false, msg);
            G::popup->showPopup(msg);
            return;
        }

        if (isLocal) {
            qDebug() << srcFun << "isLocal = true";
            dm->insert(dstLastFusedPath);

            waitUntilMetadataLoaded(3000, srcFun);

            // Selecting may trigger view/model refresh → still deferred
            sel->select(dstLastFusedPath);
        }
        else { // from Lightroom
            qDebug() << "scrFolder =" << srcFolderPath;
            if (removeRemotelyGeneratedInputImages) {
                for (const QString &path : paths) {
                    if (!QFile::remove(path)) {
                        QString msg = "Failed to remove file: " + path;
                        qWarning() << "WARNING:" << srcFun << msg;
                    }
                }
                QDir srcDir(srcFolderPath);
                if (srcDir.exists() && srcDir.isEmpty()) {
                    if (!srcDir.removeRecursively()) {
                        QString msg = "Failed to remove folder: " + srcFolderPath;
                        qWarning() << "WARNING:" << srcFun << msg;
                    }
                }
            }
            else {
                folderAndFileSelectionChange(dstLastFusedPath, "FS::threadFinished");
            }
        }

    });

    if (fsThread->isRunning()) return;

    // Start
    fsThread->start();
}

void MW::finishFocusStack(QString dstFusedImagePath)
{

}
