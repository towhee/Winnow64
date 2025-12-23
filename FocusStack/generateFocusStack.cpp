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
    focusStack(paths, method, src);
}

void MW::focusStack(const QStringList paths,
                          QString method,
                    const QString source)
{
/*
    Called locally from MW::focusStackFromSelection
    Called externally from MW::handleStartupArgs

    Aggregates paths into focus stack groups, as there might be multiple
    focus stacks in the paths list.

    Generates a fused image for each stack group.
*/
    const QString srcFun = "MW::focusStack";
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

    QList<QStringList> groups;
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

    // /* --- Debug dump with HH:mm:ss:zzz ---------------------------------------
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

    // --- Run each group ------------------------------------------------------
    qDebug() << srcFun << "groups.count() =" << groups.count();
    for (const QStringList &g : groups) {
        if (g.size() < 2) {
            qDebug().noquote() << srcFun << "Skipping group (need >=2 images):" << g;
            continue;
        }
        qDebug() << srcFun << "g.count() =" << g.count();

        generateFocusStack(g, method, srcFun);
    }
}

void MW::generateFocusStack(const QStringList paths,
                            QString method,
                            const QString source)
{
/*
    Called locally from MW::focusStack()
*/
    QString srcFun = "MW::generateFocusStack";
    if (G::isLogger) G::log(srcFun, "paths " + method);

    G::isRunningFocusStack = true;

    bool isLocal = (source == "MW::generateFocusStackFromSelection");
    isLocal = false;    // temp for debugging

    // Source images folder (used after pipeline finishes)
    QFileInfo info(paths.first());
    const QString srcFolder = info.absolutePath();
    // Source images location (ie when sourced from lightroom)
    QString root = srcFolder;
    if (!isLocal) {
        QFileInfo fi(srcFolder);
        root = fi.dir().absolutePath();
    }

    if (method.isEmpty() || method == "Default") method = "PMax";

    // Options
    // clean (send all project folders to the trash)
    bool isClean = false;       // send all project folders to the trash


    // --------------------------------------------------------------------
    // Create worker thread + FS pipeline object
    // --------------------------------------------------------------------
    QThread *thread = new QThread(this);
    fsPipeline = new FS();
    fsPipeline->moveToThread(thread);
    // local req'd for lamda
    FS *pipeline = fsPipeline.data();

    // // Set input + project root *before* thread runs
    // QString projectRoot = srcFolder + "/" + info.completeBaseName() + "_" + method;
    // pipeline->setProjectRoot(projectRoot);
    // pipeline->setInput(paths);

    // FOCUS STACK CONTROL:
    FS::Options opt;
    opt.method                  = method;
    opt.isLocal                 = false;    // pretend remote for testing

    opt.useIntermediates        = true;
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

    // Set input + project root *before* thread runs
    pipeline->setOptions(opt);
    pipeline->setInput(paths);
    QString projectRoot = srcFolder + "/" + info.completeBaseName() + "_" + method;
    pipeline->setProjectRoot(projectRoot, root);

    // When the thread starts → run the FS pipeline
    connect(thread, &QThread::started, pipeline, [pipeline, thread]()
    {
        pipeline->run();     // runs synchronously inside worker thread
        QMetaObject::invokeMethod(thread, "quit", Qt::QueuedConnection);
    });

    // Status / progress → UI
    connect(pipeline, &FS::updateStatus,
            this, &MW::updateStatus, Qt::QueuedConnection);

    connect(pipeline, &FS::progress,
            this, [this](int current, int total)
    {
        this->cacheProgressBar->updateUpperProgress(current, total, Qt::darkYellow);
    },
    Qt::QueuedConnection);

    // cleanup when finished
    connect(thread, &QThread::finished, pipeline, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread,   &QObject::deleteLater);

    // When FS finishes (thread quits)
    // We detect success by checking filesystem or pipeline signals (later)
    connect(thread, &QThread::finished, this, [=]()
    {
        // Clear progress
        cacheProgressBar->clearUpperProgress();

        // If aborted


        // Example: pick the last aligned color image as “fusedPath”
        // (temporary until focus/depth/fusion implemented)
        QString alignedDir = projectRoot + "/align";
        QDir ad(alignedDir);
        QStringList files = ad.entryList(QStringList() << "*.png",
                                         QDir::Files,
                                         QDir::Name);

        QString fusedPath;
        // if (!files.isEmpty())
        //     fusedPath = alignedDir + "/" + files.last();   // temporarily treat as result

        qDebug() << srcFun << "Finished" << fusedPath.isEmpty();

        if (!fusedPath.isEmpty())
        {
            // Copy metadata from first source using your existing logic
            ExifTool et;
            et.setOverWrite(true);
            et.copyAll(paths.first(), fusedPath);
            et.close();

            // Copy into source folder
            QFileInfo fi(fusedPath);
            QString destPath = srcFolder + "/" + fi.fileName();
            QFile::copy(fusedPath, destPath);

            // update datamodel
            dm->insert(destPath);

            // select fused image
            if (isLocal) sel->select(destPath);
            else folderAndFileSelectionChange(destPath, srcFun);

            waitUntilMetadataLoaded(5000, srcFun);

            // highlight
            setColorClassForRow(dm->currentSfRow, "Red");

            embedThumbnails();
        }
        G::isRunningFocusStack = false;
    });

    // Start
    thread->start();
}
