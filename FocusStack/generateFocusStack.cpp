#include "Main/mainwindow.h"
#include "FocusStack/fs.h"
// #include "FocusStack/Pipeline/PipelineBase.h"
// #include "FocusStack/Pipeline/pipelinepmax.h"


/*
Winnow64/FocusStack/

    Petteri/
        (untouched original sources)

    PetteriModular/
        Core/
        Tasks/
        IO/
        Wrappers/
        Namespace: FStack

    Contracts/
        IAlign.h
        IFocus.h
        IDepth.h
        IFusion.h

    Stages/
        Align/
            PetteriAlign.h/.cpp
            ECCAlign.h/.cpp
            NoOpAlign.h/.cpp
        Fusion/
            PetteriPMaxFusion.h/.cpp
            WeightedPMax.h/.cpp
            DepthAwareFusion.h/.cpp
        Depth/
            PetteriDepth.h/.cpp
            ContrastDepth.h/.cpp
        Focus/
            PetteriFocus.h/.cpp
            GradientFocus.h/.cpp

    Pipeline/
        PipelineBase.h/.cpp
        PipelinePMax.h/.cpp
        PipelineWavePMax.h/.cpp
        PipelineDepthAware.h/.cpp
        PipelineLegacyPetteri.h/.cpp

    Mask/
        maskoptions.h/.cpp
        maskgenerator.h/.cpp
        maskrefiner.h/.cpp
        maskassessor.h/.cpp

    Utils/
        pyramid.h/.cpp
        blend.h/.cpp
        filehelpers.h/.cpp

    Cache/
        PyramidCache.h/.cpp
        ImageCache.h/.cpp
*/

void MW::focusStackFromSelection()
{
    QString src = "MW::generateFocusStackFromSelection";
    if (G::isLogger) G::log(src);

    QStringList paths;
    if (!dm->getSelection(paths) || paths.isEmpty()) {
        QString msg = "No images selected for focus stacking.";
        updateStatus(false, msg, src);
        return;
    }

    QString method = "PMax";
    // QString method = "LegacyPetteri";
    generateFocusStack(paths, method, src);
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

    for (const QString &path : paths) {
        const int r = dm->rowFromPath(path);
        if (r < 0) {
            qDebug().noquote() << srcFun << "Skipping (row not found):" << path;
            continue;
        }

        const QDateTime t = dm->index(r, G::CreatedColumn).data().toDateTime();
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
    if (!current.isEmpty())
        groups.push_back(current);

    //  --- Debug dump with HH:mm:ss:zzz ---------------------------------------
    // /*
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

    /* --- Run each group ------------------------------------------------------
    qDebug() << srcFun << "groups.count() =" << groups.count();
    int n = groups.count();
    int i = 0;
    for (const QStringList &g : groups) {
        if (g.size() < 2) {
            qDebug().noquote() << srcFun << "Skipping group (need >=2 images):" << g;
            continue;
        }
        i++;
        qDebug() << srcFun << "g.count() =" << g.count();

        generateFocusStack(g, method, i, n, srcFun);
    }   */
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
                grpFolder ie 2025-11-07_0078_StreamPMax
                    alignFolder
                    focusFolder
                    depthFolder
                    fusionFolder
                    backgroundFolder
                    artifactsFolder

    srcFolder contains the input focus stack images
    dstFolder is where to save the fused result image
        - if local, use srcFolder
        - if remote, use srcFolder parent = Lightroom folder
*/
    QString srcFun = "MW::generateFocusStack";
    if (G::isLogger) G::log(srcFun, "paths " + method);

    G::isRunningFocusStack = true;

    bool isLocal = (source == "MW::generateFocusStackFromSelection");
    // isLocal = false;    // temp for debugging

    // if (method.isEmpty() || method == "Default") method = "PMax";
    method = "StreamPMax";       // align > fusePMax
    // method = "PMax1";       // align > fusePMax
    // method = "PMax2";       // align > focus > depth > fusePMax
    // method = "Ten";         // align > depth (Tenengrad focus)

    // ----------- Req'd when pipeline is finished -----------

    // Source images folder (used after pipeline finishes)
    QFileInfo info(paths.first());
    const QString srcFolder = info.absolutePath();
    // Source images location (ie when sourced from lightroom)
    QString dstFolderPath;
    if (isLocal) dstFolderPath = srcFolder;
    else {
        // get parent of srcFolder
        QFileInfo fi(srcFolder);
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

    // ----------- End section when pipeline is finished -----------

    // Options
    // clean (send all project folders to the trash)
    bool isClean = false;       // send all project folders to the trash


    // FOCUS STACK CONTROL:
    FS::Options opt;
    {
        opt.method                  = method;
        opt.isLocal                 = isLocal;    // pretend remote for testing

        opt.useIntermediates        = false;
        opt.useCache                = true;
        opt.enableOpenCL            = true;

        opt.enableAlign             = true;
        opt.enableFocusMaps         = false;
        opt.enableDepthMap          = true;
        opt.enableFusion            = true;
        opt.enableBackgroundMask    = false;
        opt.enableBackgroundReplace = false;
        opt.enableArtifactDetect    = false;

        opt.keepAlign               = true;

        opt.previewFocusMaps        = true;
        opt.keepFocusMaps           = true;

        opt.previewDepthMap         = true;
        opt.keepDepthMap            = true;

        opt.previewFusion           = true;

        opt.previewBackgroundMask   = true;
        opt.backgroundMethod        = "Depth+Focus";
    }

    // Provide a broad overview of this module, formatted for a code editor in a /*  */ comment with 80 char line width.

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
        G::isRunningFocusStack = false;

        // Clear progress
        cacheProgressBar->clearUpperProgress();

        // If aborted...

        qDebug() << srcFun << "dstLastFusedPath =" << dstLastFusedPath;

        // Evaluate we have a result path
        if (dstLastFusedPath.isEmpty())
            return;

        // folderAndFileSelectionChange(dstLastFusedPath, "FS::threadFinished");
        // ---- DEFERRED: expensive model operations ----
        // QTimer::singleShot(0, this, [=]() {

        if (isLocal) {
            qDebug() << srcFun << "isLocal = true";
            dm->insert(dstLastFusedPath);

            waitUntilMetadataLoaded(3000, srcFun);

            // Selecting may trigger view/model refresh → still deferred
            sel->select(dstLastFusedPath);
        }
        else {
            folderAndFileSelectionChange(dstLastFusedPath, "FS::threadFinished");
        }

        // });

        // if (!dstLastFusedPath.isEmpty())
        // {
            // update datamodel
            // if (isLocal) {
            //     dm->insert(dstLastFusedPath);
            //     waitUntilMetadataLoaded(3000, srcFun);
            //     // select fused image
            //     sel->select(dstLastFusedPath);
            // }
            // else folderAndFileSelectionChange(dstLastFusedPath, srcFun);
            // folderAndFileSelectionChange(dstLastFusedPath, srcFun);
        // }
    });

    qDebug() << srcFun << "xxx";
    if (fsThread->isRunning()) return;

    // Start
    fsThread->start();
}

void MW::finishFocusStack(QString dstFusedImagePath)
{

}
